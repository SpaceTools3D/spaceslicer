#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <wx/event.h>
#include <wx/timer.h>

namespace Slic3r { namespace GUI {

// SpaceSlicer: sign-in with a SpaceTools account (spacetools3d.com).
//
// Device-code flow, so the password never passes through the app: the app asks the site for a short
// code, opens the site's approval page in the browser, and polls until the signed-in user approves
// that code; the site then hands out a revocable token. The server side lives in the spacetools3d
// repository (slicer_account.py). With the token the home page shows the account, the account's model
// library (3MF models open straight into the plater) and the site's showcase.
//
// Every network call runs on a worker thread; results come back to the main thread through
// CallAfter, guarded by an alive flag, and reach the home page as window.postMessage(json).
class SpaceToolsAccount : public wxEvtHandler
{
public:
    SpaceToolsAccount();
    ~SpaceToolsAccount() override;

    void start_login();
    void cancel_login();
    void logout();
    // Sends the account state to the home page and, when signed in, refreshes the library and showcase.
    void refresh();
    void open_library_item(const std::string& id, const std::string& title);
    void open_verification_page() const;

private:
    void poll();
    void stop_polling();
    void fetch_feeds();
    void set_signed_out(bool notify);
    static void send_login_error(const std::string& message);
    std::string server() const;
    std::string token() const;
    static void send_to_home(const std::string& json);

    wxTimer                            m_poll_timer;
    std::string                        m_device_code;
    std::string                        m_user_code;
    std::string                        m_verification_uri;
    long long                          m_deadline{0};
    bool                               m_poll_in_flight{false};
    std::shared_ptr<std::atomic<bool>> m_alive{std::make_shared<std::atomic<bool>>(true)};
};

}} // namespace Slic3r::GUI
