#include "BambuStudioImport.hpp"

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <set>
#include <sstream>
#include <utility>

#include <boost/filesystem/operations.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/cstdlib.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/system/error_code.hpp>
#include <nlohmann/json.hpp>

#include <wx/colour.h>
#include <wx/dirdlg.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>
#include <wx/utils.h>

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/NetworkAgentFactory.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "MsgDialog.hpp"
#include "format.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/DialogButtons.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"

namespace fs = boost::filesystem;

namespace Slic3r { namespace GUI {

namespace {

const char* const PRESET_DIRS[3]    = {PRESET_PRINTER_NAME, PRESET_FILAMENT_NAME, PRESET_PRINT_NAME};
const char* const CODE_SECTIONS[2]  = {"access_code", "user_access_code"};
const char* const CONF_NAME         = "BambuStudio.conf";

// BambuStudio.conf is JSON, but on Windows a checksum line follows the closing brace.
nlohmann::json read_conf(const fs::path& file)
{
    boost::system::error_code ec;
    if (!fs::is_regular_file(file, ec))
        return {};
    boost::nowide::ifstream in(file.string(), std::ios::binary);
    std::stringstream       buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    const size_t      end  = text.rfind('}');
    if (end == std::string::npos)
        return {};
    nlohmann::json conf = nlohmann::json::parse(text.substr(0, end + 1), nullptr, false);
    return conf.is_discarded() || !conf.is_object() ? nlohmann::json() : conf;
}

std::string json_string(const nlohmann::json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::vector<fs::path> sorted_entries(const fs::path& dir, bool directories)
{
    std::vector<fs::path>     out;
    boost::system::error_code ec;
    if (!fs::is_directory(dir, ec))
        return out;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::path& p = it->path();
        if (directories ? fs::is_directory(p, ec) : (fs::is_regular_file(p, ec) && p.extension() == ".json"))
            out.push_back(p);
    }
    std::sort(out.begin(), out.end());
    return out;
}

PresetCollection& collection(PresetBundle& bundle, int kind)
{
    switch (kind) {
    case BambuStudioSource::Printers: return bundle.printers;
    case BambuStudioSource::Filaments: return bundle.filaments;
    default: return bundle.prints;
    }
}

} // namespace

size_t BambuStudioSource::count(Kind kind) const
{
    if (kind == Devices)
        return size_t(std::count_if(devices.begin(), devices.end(), [](const auto& d) { return !d.second.access_code.empty(); }));
    return kind < Devices ? presets[kind].size() : 0;
}

bool is_bambu_studio_dir(const fs::path& dir)
{
    boost::system::error_code ec;
    return !dir.empty() && (fs::is_regular_file(dir / CONF_NAME, ec) || fs::is_directory(dir / PRESET_USER_DIR, ec));
}

fs::path find_bambu_studio_dir()
{
    // Next to this app's own data folder first, then the platform's usual place (covers a portable install).
    std::vector<fs::path> bases{fs::path(data_dir()).parent_path()};
#if defined(_WIN32)
    if (const char* appdata = boost::nowide::getenv("APPDATA"))
        bases.emplace_back(appdata);
#elif defined(__APPLE__)
    if (const char* home = boost::nowide::getenv("HOME"))
        bases.push_back(fs::path(home) / "Library" / "Application Support");
#else
    if (const char* xdg = boost::nowide::getenv("XDG_CONFIG_HOME"))
        bases.emplace_back(xdg);
    if (const char* home = boost::nowide::getenv("HOME")) {
        bases.push_back(fs::path(home) / ".config");
        bases.push_back(fs::path(home) / ".var" / "app" / "com.bambulab.BambuStudio" / "config");
    }
#endif
    for (const fs::path& base : bases)
        for (const char* name : {"BambuStudio", "BambuStudioBeta"})
            if (is_bambu_studio_dir(base / name))
                return base / name;
    return {};
}

BambuStudioSource scan_bambu_studio(const fs::path& root)
{
    BambuStudioSource source;
    source.root = root;

    // user/<account or "default">/<machine|filament|process>[/base]/*.json. The .info files next to them hold
    // Bambu cloud sync ids and are left behind, so the copies start out as local presets here.
    for (int kind = 0; kind < 3; ++kind) {
        std::set<std::string> seen;
        for (const fs::path& account : sorted_entries(root / PRESET_USER_DIR, true))
            for (const char* sub : {"base", ""}) {
                fs::path dir = account / PRESET_DIRS[kind];
                if (*sub)
                    dir /= sub;
                for (const fs::path& file : sorted_entries(dir, false))
                    if (seen.insert(std::string(sub) + "/" + file.filename().string()).second)
                        source.presets[kind].push_back(file);
            }
    }

    const nlohmann::json conf = read_conf(root / CONF_NAME);
    for (int s = 0; s < 2; ++s) {
        const auto it = conf.find(CODE_SECTIONS[s]);
        if (it == conf.end() || !it->is_object())
            continue;
        for (auto code = it->begin(); code != it->end(); ++code)
            if (code->is_string() && !code->get<std::string>().empty())
                source.codes[s][code.key()] = code->get<std::string>();
    }
    const auto machines = conf.find("local_machines");
    if (machines != conf.end() && machines->is_object())
        for (auto it = machines->begin(); it != machines->end(); ++it) {
            if (!it->is_object() || it.key().empty())
                continue;
            BBLocalMachine machine;
            machine.dev_id       = it.key();
            machine.dev_name     = json_string(*it, "dev_name");
            machine.dev_ip       = json_string(*it, "dev_ip");
            machine.printer_type = json_string(*it, "printer_type");
            machine.access_code  = json_string(*it, "access_code");
            for (int s = 0; s < 2 && machine.access_code.empty(); ++s) {
                const auto code = source.codes[s].find(machine.dev_id);
                if (code != source.codes[s].end())
                    machine.access_code = code->second;
            }
            source.devices[machine.dev_id] = machine;
        }
    return source;
}

BambuStudioImportResult import_bambu_studio(const BambuStudioSource& source, const bool (&kinds)[BambuStudioSource::KindCount])
{
    BambuStudioImportResult result;
    GUI_App&      app    = wxGetApp();
    PresetBundle& bundle = *app.preset_bundle;
    AppConfig&    config = *app.app_config;

    std::string folder = config.get("preset_folder");
    if (folder.empty())
        folder = DEFAULT_USER_FOLDER_NAME;
    const fs::path user_dir = fs::path(data_dir()) / PRESET_USER_DIR / folder;

    std::vector<std::pair<int, fs::path>> copied;
    for (int kind = 0; kind < 3; ++kind) {
        if (!kinds[kind])
            continue;
        for (const fs::path& file : source.presets[kind]) {
            fs::path dest = user_dir / PRESET_DIRS[kind];
            if (file.parent_path().filename() == "base")
                dest /= "base";
            dest /= file.filename();
            boost::system::error_code ec;
            if (collection(bundle, kind).find_preset(file.stem().string()) != nullptr || fs::exists(dest, ec)) {
                ++result.skipped;
                continue;
            }
            fs::create_directories(dest.parent_path(), ec);
            fs::copy_file(file, dest, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Bambu Studio import: cannot copy " << file.string() << ": " << ec.message();
                ++result.unusable;
                continue;
            }
            copied.emplace_back(kind, dest);
        }
    }

    if (!copied.empty()) {
        try {
            bundle.load_user_presets(folder, ForwardCompatibilitySubstitutionRule::Enable);
        } catch (const std::exception& err) {
            BOOST_LOG_TRIVIAL(error) << "Bambu Studio import: loading presets reported " << err.what();
        }
        // A copy that did not load (its parent preset does not exist here) is taken out again, so it does
        // not log an error on every start.
        for (const auto& [kind, dest] : copied) {
            const Preset* preset = collection(bundle, kind).find_preset(dest.stem().string());
            if (preset != nullptr && preset->is_user()) {
                ++result.loaded[kind];
            } else {
                boost::system::error_code ec;
                fs::remove(dest, ec);
                ++result.unusable;
            }
        }
        if (app.mainframe)
            app.mainframe->update_side_preset_ui();
    }

    if (kinds[BambuStudioSource::Devices]) {
        for (const auto& [dev_id, machine] : source.devices) {
            if (machine.access_code.empty() || config.get_local_machines().count(dev_id))
                continue;
            BBLocalMachine copy   = machine;
            copy.printer_agent_id = BBL_PRINTER_AGENT_ID;
            config.update_local_machine(copy);
            ++result.devices;
        }
        // Codes of printers Bambu Studio reached over the cloud, used when this app finds them on the LAN.
        for (int s = 0; s < 2; ++s)
            for (const auto& [dev_id, code] : source.codes[s])
                if (config.get(CODE_SECTIONS[s], dev_id).empty())
                    config.set_str(CODE_SECTIONS[s], dev_id, code);
        config.save();
    }
    return result;
}

BambuStudioImportDialog::BambuStudioImportDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("Import from Bambu Studio"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    SetBackgroundColour(*wxWHITE);
    SetFont(Label::Body_14);

    const int     pad   = FromDIP(20);
    const wxColour muted = StateColor::darkModeColorFor(wxColour("#6B6B6A"));
    auto*         sizer = new wxBoxSizer(wxVERTICAL);

    auto* intro = new Label(this, Label::Body_13,
                            _L("Copies your profiles and LAN printers into this app. Bambu Studio's own files are not changed."));
    intro->SetForegroundColour(muted);
    intro->Wrap(FromDIP(400));
    sizer->Add(intro, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto* path_row = new wxBoxSizer(wxHORIZONTAL);
    m_path = new Label(this, Label::Body_12, wxEmptyString, wxST_ELLIPSIZE_MIDDLE, wxSize(FromDIP(300), -1));
    m_path->SetForegroundColour(muted);
    auto* change = new Button(this, _L("Choose folder"));
    change->SetStyle(ButtonStyle::Regular, ButtonType::Compact);
    change->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { choose_folder(); });
    path_row->Add(m_path, 1, wxALIGN_CENTER_VERTICAL);
    path_row->Add(change, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    sizer->Add(path_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_empty = new Label(this, Label::Body_13, _L("No Bambu Studio profiles or printers were found here."));
    m_empty->SetForegroundColour(muted);
    sizer->Add(m_empty, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    const wxString names[BambuStudioSource::KindCount] = {_L("Printer profiles"), _L("Filament profiles"), _L("Process profiles"),
                                                          _L("LAN printers and access codes")};
    auto* rows = new wxFlexGridSizer(2, FromDIP(12), FromDIP(16));
    rows->AddGrowableCol(0);
    for (int kind = 0; kind < BambuStudioSource::KindCount; ++kind) {
        auto* row      = new wxBoxSizer(wxHORIZONTAL);
        m_checks[kind] = new ::CheckBox(this);
        m_checks[kind]->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) { update_buttons(); e.Skip(); });
        auto* name = new Label(this, Label::Body_14, names[kind]);
        name->Bind(wxEVT_LEFT_DOWN, [this, kind](wxMouseEvent&) {
            if (m_checks[kind]->IsEnabled()) {
                m_checks[kind]->SetValue(!m_checks[kind]->GetValue());
                update_buttons();
            }
        });
        row->Add(m_checks[kind], 0, wxALIGN_CENTER_VERTICAL);
        row->Add(name, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        m_counts[kind] = new Label(this, Label::Body_14, wxEmptyString);
        m_counts[kind]->SetForegroundColour(muted);
        rows->Add(row, 0, wxEXPAND);
        rows->Add(m_counts[kind], 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    }
    sizer->Add(rows, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_buttons = new DialogButtons(this, {"Import", "Cancel"}, _L("Import"));
    m_buttons->GetButtonFromIndex(0)->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { run_import(); });
    m_buttons->GetCANCEL()->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    sizer->Add(m_buttons, 0, wxEXPAND | wxTOP, pad);

    SetSizerAndFit(sizer);
    set_source(find_bambu_studio_dir());
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

void BambuStudioImportDialog::set_source(const fs::path& root)
{
    m_source = root.empty() ? BambuStudioSource() : scan_bambu_studio(root);
    m_path->SetLabel(root.empty() ? _L("Bambu Studio was not found on this computer.") : from_path(root));
    m_path->SetToolTip(root.empty() ? wxString() : from_path(root));

    size_t total = 0;
    for (int kind = 0; kind < BambuStudioSource::KindCount; ++kind) {
        const size_t n = m_source.count(BambuStudioSource::Kind(kind));
        total += n;
        m_counts[kind]->SetLabel(wxString::Format("%zu", n));
        m_checks[kind]->SetValue(n > 0);
        m_checks[kind]->Enable(n > 0);
    }
    m_empty->Show(!root.empty() && total == 0);
    update_buttons();
    Layout();
    Fit();
}

void BambuStudioImportDialog::choose_folder()
{
    const fs::path start = m_source.root.empty() ? fs::path(data_dir()).parent_path() : m_source.root;
    wxDirDialog    dlg(this, _L("Choose the Bambu Studio data folder"), from_path(start), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK)
        return;
    const fs::path picked(into_path(dlg.GetPath()));
    // The user may pick the account folder or the user folder inside it; walk up to the data folder.
    for (fs::path dir = picked; !dir.empty(); dir = dir.parent_path()) {
        if (is_bambu_studio_dir(dir)) {
            set_source(dir);
            return;
        }
        if (dir == dir.parent_path())
            break;
    }
    set_source(picked);
}

void BambuStudioImportDialog::update_buttons()
{
    bool any = false;
    for (int kind = 0; kind < BambuStudioSource::KindCount; ++kind)
        any = any || (m_checks[kind]->GetValue() && m_source.count(BambuStudioSource::Kind(kind)) > 0);
    m_buttons->GetButtonFromIndex(0)->Enable(any);
}

void BambuStudioImportDialog::run_import()
{
    bool kinds[BambuStudioSource::KindCount];
    for (int kind = 0; kind < BambuStudioSource::KindCount; ++kind)
        kinds[kind] = m_checks[kind]->GetValue();

    BambuStudioImportResult result;
    {
        wxBusyCursor busy;
        result = import_bambu_studio(m_source, kinds);
    }

    std::vector<wxString> lines;
    if (result.loaded[0] > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% printer profile imported.", "%1% printer profiles imported.", result.loaded[0]), result.loaded[0]));
    if (result.loaded[1] > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% filament profile imported.", "%1% filament profiles imported.", result.loaded[1]), result.loaded[1]));
    if (result.loaded[2] > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% process profile imported.", "%1% process profiles imported.", result.loaded[2]), result.loaded[2]));
    if (result.devices > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% LAN printer imported. It appears on the Device tab after a restart.",
                                               "%1% LAN printers imported. They appear on the Device tab after a restart.", result.devices),
                                     result.devices));
    if (result.skipped > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% profile was skipped because one with the same name already exists.",
                                               "%1% profiles were skipped because ones with the same names already exist.", result.skipped),
                                     result.skipped));
    if (result.unusable > 0)
        lines.push_back(format_wxstr(_L_PLURAL("%1% profile could not be used because the profile it is based on is missing.",
                                               "%1% profiles could not be used because the profiles they are based on are missing.",
                                               result.unusable),
                                     result.unusable));
    if (lines.empty())
        lines.push_back(_L("There was nothing new to import."));

    wxString message;
    for (const wxString& line : lines)
        message += (message.empty() ? "" : "\n") + line;
    MessageDialog(this, message, _L("Import from Bambu Studio"), wxOK).ShowModal();
    EndModal(wxID_OK);
}

void BambuStudioImportDialog::on_dpi_changed(const wxRect&)
{
    Layout();
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
