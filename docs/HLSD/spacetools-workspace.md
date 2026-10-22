# SpaceTools workspace

The bundled home and setup pages provide SpaceSlicer's own workspace, branding and navigation.
They keep the existing native project commands and printer setup flow. Their text comes from
`home_page_strings()` and `guide_strings()`, through the existing gettext catalogs and `T()` bridge.
The Turkish catalog includes the SpaceSlicer messages; other languages use English where a
translation is unavailable. Recent project names and paths are inserted as data, not HTML.

## SpaceTools account

`WebViewPanel` keeps a separate native webview for `https://spacetools3d.com`. The account button
opens the website's `/login` page, so the existing website handles credentials, sign-in, sign-out
and the cookie session. Library and gallery open `/stl-library` and `/gallery` in this same view.
This is a website session: the desktop does not store an access token, claim a connected account,
or use this session to synchronize printer presets. Passwords and website cookies are never
passed to an Orca or Bambu service agent.

The local homepage sends only named commands. Native code chooses fixed routes and accepts these
commands only from the bundled homepage. The remote view consumes its own events and never
dispatches page messages to the local file/printer API. Same-origin website navigation stays
inside the view; other HTTPS sites open in the system browser. External OAuth sign-in needs a
separate browser-to-desktop handoff before it can share a session with this view.

The webview remains alive when returning to local projects. On Linux its cookie store is
`cache/spacetools_cookies.db`, separate from the printer cloud cookie store. Stealth Mode blocks
opening the online workspace and destroys an existing view so it cannot keep making requests.
Local slicing needs no account. OrcaCloud and Bambu Cloud remain independent optional printer
connections inside a collapsed section of the home sidebar.

## Verification

`node scripts/spaceslicer_branding/test_workspace.js` checks the command routing, workspace/cloud
transitions, recent-project empty state, literal project names and keyboard opening. The app's
normal Release builds validate native webview integration on Windows, macOS and Linux. Check the
home page in both English and Turkish, and the setup page at its 820 by 660 DIP window size.
