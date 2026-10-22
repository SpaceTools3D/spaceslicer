#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "libslic3r/AppConfig.hpp"
#include "GUI_Utils.hpp"

class wxWindow;
class Label;
class CheckBox;

namespace Slic3r { namespace GUI {

class DialogButtons;

// SpaceSlicer: moving over from Bambu Studio. Copies the user presets (printer, filament, process) and the
// LAN printers with their access codes out of a Bambu Studio data folder. Both apps use the same preset
// format, so the JSON files are copied as they are; Bambu Studio's own files are only read.
struct BambuStudioSource
{
    enum Kind { Printers, Filaments, Processes, Devices, KindCount };

    boost::filesystem::path              root;
    std::vector<boost::filesystem::path> presets[3];   // indexed by Printers, Filaments, Processes
    std::map<std::string, BBLocalMachine> devices;     // dev_id -> LAN printer, access code filled in when known
    std::map<std::string, std::string>   codes[2];     // the flat "access_code" and "user_access_code" sections

    size_t count(Kind kind) const;
};

struct BambuStudioImportResult
{
    int loaded[3]{0, 0, 0};
    int skipped{0};    // a preset with that name already exists
    int unusable{0};   // copied, but SpaceSlicer cannot load it (its parent preset is missing); removed again
    int devices{0};
};

// The Bambu Studio data folder of this user, or an empty path when there is none.
boost::filesystem::path find_bambu_studio_dir();
bool                    is_bambu_studio_dir(const boost::filesystem::path& dir);
BambuStudioSource       scan_bambu_studio(const boost::filesystem::path& root);
BambuStudioImportResult import_bambu_studio(const BambuStudioSource& source, const bool (&kinds)[BambuStudioSource::KindCount]);

class BambuStudioImportDialog : public DPIDialog
{
public:
    explicit BambuStudioImportDialog(wxWindow* parent);

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    void set_source(const boost::filesystem::path& root);
    void choose_folder();
    void update_buttons();
    void run_import();

    BambuStudioSource m_source;
    Label*            m_path{nullptr};
    Label*            m_empty{nullptr};
    ::CheckBox*       m_checks[BambuStudioSource::KindCount]{};
    Label*            m_counts[BambuStudioSource::KindCount]{};
    DialogButtons*    m_buttons{nullptr};
};

}} // namespace Slic3r::GUI
