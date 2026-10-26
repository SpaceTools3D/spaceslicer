#include "SpaceToolsAccount.hpp"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>

#include <boost/log/trivial.hpp>
#include <nlohmann/json.hpp>
#include <wx/string.h>
#include <wx/utils.h>

#include "libslic3r/AppConfig.hpp"
#include "slic3r/Utils/Http.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "WebViewDialog.hpp"

namespace Slic3r { namespace GUI {

using json = nlohmann::json;

static const char* DEFAULT_SERVER = "https://spacetools3d.com";
static const char* CFG_TOKEN      = "spacetools_token";
static const char* CFG_USER       = "spacetools_user";
static const char* CFG_SERVER     = "spacetools_server";

static long long now_s() { return static_cast<long long>(std::time(nullptr)); }

SpaceToolsAccount::SpaceToolsAccount()
{
    m_poll_timer.SetOwner(this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, m_poll_timer.GetId());
}

SpaceToolsAccount::~SpaceToolsAccount()
{
    *m_alive = false;
    m_poll_timer.Stop();
}

std::string SpaceToolsAccount::server() const
{
    // A test server can be set by hand in the app config; anything that is not https is ignored.
    const std::string custom = wxGetApp().app_config->get(CFG_SERVER);
    return custom.rfind("https://", 0) == 0 ? custom : std::string(DEFAULT_SERVER);
}

std::string SpaceToolsAccount::token() const { return wxGetApp().app_config->get(CFG_TOKEN); }

void SpaceToolsAccount::send_to_home(const std::string& msg)
{
    if (WebViewPanel* home = WebViewPanel::if_built())
        home->RunScript(wxString::Format("window.postMessage(%s)", from_u8(msg)));
}

void SpaceToolsAccount::send_login_error(const std::string& message)
{
    send_to_home(json{{"command", "spacetools_login_error"}, {"data", {{"message", message}}}}.dump());
}

void SpaceToolsAccount::open_verification_page() const
{
    if (!m_verification_uri.empty())
        wxLaunchDefaultBrowser(from_u8(m_verification_uri));
}

void SpaceToolsAccount::start_login()
{
    if (m_poll_timer.IsRunning()) {          // a code is already waiting for approval: show it again
        send_to_home(json{{"command", "spacetools_login_code"}, {"data", {{"user_code", m_user_code}, {"uri", m_verification_uri}}}}.dump());
        open_verification_page();
        return;
    }
    auto alive = m_alive;
    Http::post(server() + "/api/slicer/device/start")
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .timeout_connect(10)
        .timeout_max(20)
        .set_post_body(std::string("{}"))
        .on_complete([this, alive](std::string body, unsigned /*status*/) {
            wxGetApp().CallAfter([this, alive, body]() {
                if (!*alive)
                    return;
                const json j = json::parse(body, nullptr, false);
                if (j.is_discarded() || !j.contains("device_code") || !j.contains("user_code") || !j.contains("verification_uri")) {
                    send_login_error(_u8L("SpaceTools did not answer as expected. Try again in a moment."));
                    return;
                }
                m_device_code      = j["device_code"].get<std::string>();
                m_user_code        = j["user_code"].get<std::string>();
                m_verification_uri = j["verification_uri"].get<std::string>();
                const int interval = std::max(2, j.value("interval", 3));
                m_deadline         = now_s() + j.value("expires_in", 600);
                send_to_home(json{{"command", "spacetools_login_code"}, {"data", {{"user_code", m_user_code}, {"uri", m_verification_uri}}}}.dump());
                open_verification_page();
                m_poll_timer.Start(interval * 1000);
            });
        })
        .on_error([alive](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(warning) << "SpaceTools sign-in start failed: " << status << " " << error;
            wxGetApp().CallAfter([alive, status]() {
                if (!*alive)
                    return;
                send_login_error(status == 429 ? _u8L("Too many attempts. Wait a few minutes and try again.")
                                               : _u8L("Could not reach spacetools3d.com. Check your internet connection."));
            });
        })
        .perform();
}

void SpaceToolsAccount::stop_polling()
{
    m_poll_timer.Stop();
    m_device_code.clear();
    m_poll_in_flight = false;
}

void SpaceToolsAccount::cancel_login() { stop_polling(); }

void SpaceToolsAccount::poll()
{
    if (m_device_code.empty() || m_poll_in_flight)
        return;
    if (now_s() > m_deadline) {
        stop_polling();
        send_login_error(_u8L("The code has expired. Sign in again to get a new one."));
        return;
    }
    m_poll_in_flight = true;
    auto alive = m_alive;
    Http::post(server() + "/api/slicer/device/poll")
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .timeout_connect(10)
        .timeout_max(20)
        .set_post_body(json{{"device_code", m_device_code}}.dump())
        .on_complete([this, alive](std::string body, unsigned /*status*/) {
            wxGetApp().CallAfter([this, alive, body]() {
                if (!*alive)
                    return;
                m_poll_in_flight = false;
                if (m_device_code.empty())    // cancelled meanwhile
                    return;
                const json j = json::parse(body, nullptr, false);
                const std::string state = j.is_discarded() ? std::string() : j.value("status", "");
                if (state == "approved" && j.contains("token") && j.contains("user")) {
                    stop_polling();
                    AppConfig* cfg = wxGetApp().app_config;
                    cfg->set(CFG_TOKEN, j["token"].get<std::string>());
                    cfg->set(CFG_USER, j["user"].dump());
                    cfg->save();
                    send_to_home(json{{"command", "spacetools_userlogin"}, {"data", j["user"]}}.dump());
                    fetch_feeds();
                } else if (state == "denied") {
                    stop_polling();
                    send_login_error(_u8L("Sign-in was cancelled on the website."));
                } else if (state == "expired") {
                    stop_polling();
                    send_login_error(_u8L("The code has expired. Sign in again to get a new one."));
                } else if (state == "slow_down") {
                    m_poll_timer.Start(std::max(3, j.value("interval", 5)) * 1000);
                }
                // "pending" or an unreadable answer: keep waiting until the code expires
            });
        })
        .on_error([this, alive](std::string, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(warning) << "SpaceTools sign-in poll failed: " << status << " " << error;
            wxGetApp().CallAfter([this, alive]() {
                if (*alive)
                    m_poll_in_flight = false;   // a dropped connection is retried on the next tick
            });
        })
        .perform();
}

void SpaceToolsAccount::set_signed_out(bool notify)
{
    AppConfig* cfg = wxGetApp().app_config;
    cfg->set(CFG_TOKEN, std::string());
    cfg->set(CFG_USER, std::string());
    cfg->save();
    if (notify)
        send_to_home(json{{"command", "spacetools_useroffline"}}.dump());
}

void SpaceToolsAccount::logout()
{
    const std::string tok = token();
    if (!tok.empty()) {
        // Best effort: the token is forgotten here whatever the site answers.
        Http::post(server() + "/api/slicer/logout")
            .header("Authorization", "Bearer " + tok)
            .header("Accept", "application/json")
            .timeout_max(10)
            .set_post_body(std::string("{}"))
            .on_complete([](std::string, unsigned) {})
            .on_error([](std::string, std::string, unsigned) {})
            .perform();
    }
    stop_polling();
    set_signed_out(true);
}

void SpaceToolsAccount::refresh()
{
    const std::string tok = token();
    if (tok.empty()) {
        send_to_home(json{{"command", "spacetools_useroffline"}}.dump());
        return;
    }
    // Show the remembered account at once; the site confirms (or rejects) the token right after.
    const json cached = json::parse(wxGetApp().app_config->get(CFG_USER), nullptr, false);
    if (!cached.is_discarded() && cached.is_object())
        send_to_home(json{{"command", "spacetools_userlogin"}, {"data", cached}}.dump());

    auto alive = m_alive;
    Http::get(server() + "/api/slicer/me")
        .header("Authorization", "Bearer " + tok)
        .header("Accept", "application/json")
        .timeout_connect(10)
        .timeout_max(20)
        .on_complete([this, alive, tok](std::string body, unsigned) {
            wxGetApp().CallAfter([this, alive, tok, body]() {
                if (!*alive || token() != tok)
                    return;
                const json j = json::parse(body, nullptr, false);
                if (!j.is_discarded() && j.contains("user")) {
                    wxGetApp().app_config->set(CFG_USER, j["user"].dump());
                    send_to_home(json{{"command", "spacetools_userlogin"}, {"data", j["user"]}}.dump());
                }
                fetch_feeds();
            });
        })
        .on_error([this, alive, tok](std::string, std::string error, unsigned status) {
            wxGetApp().CallAfter([this, alive, tok, status]() {
                if (!*alive || token() != tok)
                    return;
                if (status == 401)        // revoked on the website: sign out here too
                    set_signed_out(true);
                // offline: keep the remembered account, the next refresh tries again
            });
        })
        .perform();
}

void SpaceToolsAccount::fetch_feeds()
{
    const std::string tok = token();
    if (tok.empty())
        return;
    for (const char* kind : {"library", "gallery"}) {
        auto alive = m_alive;
        const std::string k = kind;
        Http::get(server() + "/api/slicer/" + k)
            .header("Authorization", "Bearer " + tok)
            .header("Accept", "application/json")
            .timeout_connect(10)
            .timeout_max(30)
            .on_complete([alive, k](std::string body, unsigned) {
                wxGetApp().CallAfter([alive, k, body]() {
                    if (!*alive)
                        return;
                    const json j = json::parse(body, nullptr, false);
                    if (j.is_discarded() || !j.contains("items") || !j["items"].is_array())
                        return;
                    send_to_home(json{{"command", "spacetools_feed"}, {"data", {{"kind", k}, {"items", j["items"]}}}}.dump());
                });
            })
            .on_error([k](std::string, std::string error, unsigned status) {
                BOOST_LOG_TRIVIAL(warning) << "SpaceTools " << k << " feed failed: " << status << " " << error;
            })
            .perform();
    }
}

void SpaceToolsAccount::open_library_item(const std::string& id, const std::string& title)
{
    const std::string tok = token();
    if (tok.empty() || id.empty())
        return;
    auto alive = m_alive;
    Http::post(server() + "/api/slicer/library/" + Http::url_encode(id) + "/open")
        .header("Authorization", "Bearer " + tok)
        .header("Accept", "application/json")
        .timeout_connect(10)
        .timeout_max(20)
        .set_post_body(std::string("{}"))
        .on_complete([alive, title](std::string body, unsigned) {
            wxGetApp().CallAfter([alive, title, body]() {
                if (!*alive)
                    return;
                const json j = json::parse(body, nullptr, false);
                const std::string url = j.is_discarded() ? std::string() : j.value("url", "");
                if (url.rfind("https://", 0) != 0)
                    return;
                // Plater::import_model_id splits "<url>&name=<file name>" and imports the download.
                wxGetApp().request_model_download(from_u8(url + "&name=" + (title.empty() ? std::string("model") : title) + ".3mf"));
            });
        })
        .on_error([alive](std::string, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(warning) << "SpaceTools model open failed: " << status << " " << error;
            wxGetApp().CallAfter([alive]() {
                if (*alive)
                    show_error(nullptr, _u8L("This model could not be opened. Try again, or download it from spacetools3d.com."));
            });
        })
        .perform();
}

}} // namespace Slic3r::GUI
