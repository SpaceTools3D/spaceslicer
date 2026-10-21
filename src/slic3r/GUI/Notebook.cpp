#include "Notebook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"
#include "libslic3r_version.h"

//BBS set font size

#include <wx/bookctrl.h>
#include <cmath>
#include <cstddef>
#include <utility>
#include <string>
#include <algorithm>
#include <wx/button.h>
#include <wx/colour.h>
#include <wx/dcclient.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/window.h>

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

// SpaceSlicer tab bar. The bar is the same deep violet in light and dark mode, so these colours are
// fixed rather than palette colours (the home page's dark tokens: --hp-violet, --hp-text).
static const wxColour TAB_BAR_BG("#1A1424");
static const wxColour TAB_TILE_BG("#2A2336");   // hover, and the resting look of an icon-only tab
static const wxColour TAB_SELECTED_BG("#8B63F0");
static const wxColour TAB_TEXT("#F1EEF6");

static wxSize tab_button_min_size(bool icon_only, int em)
{
    return {(icon_only ? 52 : 120) * em / 10, 40 * em / 10};
}

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent, wxBoxSizer* side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    SetBackgroundColour(TAB_BAR_BG);

    int em = em_unit(this);// Slic3r::GUI::wxGetApp().em_unit();
    // SpaceSlicer: tabs are separate pills with a small gap
    m_btn_margin = std::lround(0.4 * em);
    m_line_margin = std::lround(0.1 * em);

    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    this->SetSizer(m_sizer);

    // SpaceSlicer: the logo and the app name lead the bar. A Button keeps its own colours through the
    // dark-mode walk, which a wxStaticText would not; it has no action of its own.
    m_brand = new Button(this, wxString::FromUTF8(SLIC3R_APP_NAME), "OrcaSlicer", wxNO_BORDER, 30);
    m_brand->SetFont(Label::Head_20);
    m_brand->SetIconSpacing(FromDIP(10));
    m_brand->SetCornerRadius(0);
    m_brand->SetBackgroundColor(StateColor(std::pair{TAB_BAR_BG, (int) StateColor::Normal}));
    m_brand->SetBorderColor(StateColor(std::pair{TAB_BAR_BG, (int) StateColor::Normal}));
    m_brand->SetTextColor(StateColor(std::pair{TAB_TEXT, (int) StateColor::Normal}));
    m_brand->SetCanFocus(false);
    m_sizer->Add(m_brand, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(12));

    m_buttons_sizer = new wxFlexGridSizer(1, m_btn_margin, m_btn_margin);
    m_sizer->Add(m_buttons_sizer, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(8));

    if (side_tools != NULL) {
        m_sizer->AddStretchSpacer(1);
        for (size_t idx = 0; idx < side_tools->GetItemCount(); idx++) {
            wxSizerItem* item = side_tools->GetItem(idx);
            wxWindow* item_win = item->GetWindow();
            if (item_win) {
                item_win->Reparent(this);
            }
        }
        m_sizer->Add(side_tools, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    }

    // BBS: disable custom paint
    //this->Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, [](auto& e){
    });
}

void ButtonsListCtrl::OnPaint(wxPaintEvent&)
{
    //Slic3r::GUI::wxGetApp().UpdateDarkUI(this);
    const wxSize sz = GetSize();
    wxPaintDC dc(this);

    if (m_selection < 0 || m_selection >= (int)m_pageButtons.size())
        return;

    wxColour selected_btn_bg("#1F8EEA");
    wxColour default_btn_bg("#1A1424"); // Gradient #414B4E
    const wxColour& btn_marker_color = Slic3r::GUI::wxGetApp().get_color_hovered_btn_label();

    // highlight selected notebook button

    for (int idx = 0; idx < int(m_pageButtons.size()); idx++) {
        Button* btn = m_pageButtons[idx];

        btn->SetBackgroundColor(idx == m_selection ? selected_btn_bg : default_btn_bg);

        wxPoint pos = btn->GetPosition();
        wxSize size = btn->GetSize();
        const wxColour& clr = idx == m_selection ? btn_marker_color : default_btn_bg;
        dc.SetPen(clr);
        dc.SetBrush(clr);
        dc.DrawRectangle(pos.x, pos.y + size.y, size.x, sz.y - size.y);
    }
    // Draw orange bottom line

    dc.SetPen(btn_marker_color);
    dc.SetBrush(btn_marker_color);
    dc.DrawRectangle(1, sz.y - m_line_margin, sz.x, m_line_margin);
}

void ButtonsListCtrl::UpdateMode()
{
    //m_mode_sizer->SetMode(Slic3r::GUI::wxGetApp().get_mode());
}

void ButtonsListCtrl::Rescale()
{
    //m_mode_sizer->msw_rescale();
    int em = em_unit(this);
    for (Button* btn : m_pageButtons) {
        btn->SetMinSize(tab_button_min_size(btn->GetLabel().empty(), em));
        btn->SetCornerRadius(FromDIP(10));
        btn->Rescale();
    }
    if (m_brand)
        m_brand->Rescale();

    // BBS: no gap
    //m_btn_margin = std::lround(0.3 * em);
    //m_line_margin = std::lround(0.1 * em);
    //m_buttons_sizer->SetVGap(m_btn_margin);
    //m_buttons_sizer->SetHGap(m_btn_margin);

    m_sizer->Layout();
}

void ButtonsListCtrl::StyleButton(size_t n)
{
    if (n >= m_pageButtons.size())
        return;
    Button* btn = m_pageButtons[n];
    const bool selected = static_cast<int>(n) == m_selection;
    const wxColour& rest = btn->GetLabel().empty() ? TAB_TILE_BG : TAB_BAR_BG;
    StateColor bg = selected ? StateColor(std::pair{TAB_SELECTED_BG, (int) StateColor::Normal})
                             : StateColor(std::pair{TAB_TILE_BG, (int) StateColor::Hovered}, std::pair{rest, (int) StateColor::Normal});
    btn->SetBackgroundColor(bg);
    btn->SetBorderColor(bg);
    btn->SetTextColor(StateColor(std::pair{TAB_TEXT, (int) StateColor::Normal}));
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel && sel >= 0 && sel < static_cast<int>(m_pageButtons.size()))
        return;

    const int previous = m_selection;
    m_selection = (sel < 0 || sel >= static_cast<int>(m_pageButtons.size())) ? -1 : sel;
    if (previous >= 0)
        StyleButton(previous);
    if (m_selection >= 0)
        StyleButton(m_selection);

    Refresh();
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/, const wxBitmap &bmp /* = wxNullBitmap */)
{
    Button * btn = new Button(this, text, bmp_name, wxNO_BORDER);
    btn->SetCornerRadius(FromDIP(10));

    if (bmp_name.empty() && bmp.IsOk())
        btn->SetIcon(bmp);

    // The label no longer carries a leading space, so widen the icon<->text gap to keep the
    // original spacing between a tab's icon and its caption.
    {
        wxClientDC dc(btn);
        dc.SetFont(btn->GetFont());
        int space_w = 0;
        dc.GetTextExtent(" ", &space_w, nullptr);
        btn->SetIconSpacing(5 + space_w);
    }

    int em = em_unit(this);
    btn->SetMinSize(tab_button_min_size(text.empty(), em));
    btn->SetPaddingSize(wxSize(FromDIP(16), FromDIP(8)));
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            //do it later
            //SetSelection(sel);
            
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    if (m_selection >= static_cast<int>(n))
        ++m_selection;
    StyleButton(n);
    m_pageLabels.insert(m_pageLabels.begin() + n, text); // ORCA
    m_pageIcons.insert(m_pageIcons.begin() + n, bmp_name);
    m_buttons_sizer->Insert(n, new wxSizerItem(btn));
    m_buttons_sizer->SetCols(m_buttons_sizer->GetCols() + 1);
    m_sizer->Layout();
    return true;
}

void ButtonsListCtrl::RemovePage(size_t n)
{
    if (n >= m_pageButtons.size())
        return;

    if (m_selection == static_cast<int>(n))
        m_selection = -1;
    else if (m_selection > static_cast<int>(n))
        --m_selection;

    Button* btn = m_pageButtons[n];
    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_pageLabels.erase(m_pageLabels.begin() + n); // ORCA
    m_pageIcons.erase(m_pageIcons.begin() + n);
    m_buttons_sizer->Remove(n);
#if __WXOSX__
    RemoveChild(btn);
#else
    btn->Reparent(nullptr);
#endif
    btn->Destroy();
    m_sizer->Layout();
}

bool ButtonsListCtrl::SetPageImage(size_t n, const std::string& bmp_name) const
{
    if (n >= m_pageButtons.size())
        return false;
     
    // BBS
    //return m_pageButtons[n]->SetBitmap_(bmp_name);
    ScalableBitmap bitmap(NULL, bmp_name);
    //m_pageButtons[n]->SetBitmap_(bitmap);
    return true;
}

void ButtonsListCtrl::SetPageText(size_t n, const wxString& strText)
{
    Button* btn = m_pageButtons[n];
    btn->SetLabel(strText);
    if(!strText.empty())  // ORCA
        m_pageLabels[n] = strText;
}

// ORCA
void ButtonsListCtrl::SetCompact(size_t n, bool compact)
{
    int em = em_unit(this);
    Button* btn = m_pageButtons[n];
    btn->SetMinSize(tab_button_min_size(compact, em));
    btn->SetLabel(compact ? "" : m_pageLabels[n]);
    StyleButton(n);
}

wxString ButtonsListCtrl::GetPageText(size_t n) const
{
    Button* btn = m_pageButtons[n];
    return btn->GetLabel();
}

// ORCA
wxString ButtonsListCtrl::GetPageLabel(size_t n) const
{
    return n < m_pageLabels.size() ? m_pageLabels[n] : wxString();
}

// ORCA
void ButtonsListCtrl::SetOverflowButton(wxWindow* button)
{
    if (m_overflow_button == button)
        return;

    if (m_overflow_button != nullptr)
        m_sizer->Detach(m_overflow_button);

    m_overflow_button = button;

    if (m_overflow_button != nullptr)
        // Right after the brand (index 0) and the tab buttons (index 1), ahead of any stretch spacer / side_tools.
        m_sizer->Insert(2, m_overflow_button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, m_btn_margin);

    m_sizer->Layout();
}

//#endif // _WIN32

void Notebook::Init()
{
    // We don't need any border as we don't have anything to separate the
    // page contents from.
    SetInternalBorder(0);

    // No effects by default.
    m_showEffect = m_hideEffect = wxSHOW_EFFECT_NONE;

    m_showTimeout = m_hideTimeout = 0;

    m_pageNames.clear();

    /* On Linux, Gstreamer wxMediaCtrl does not seem to get along well with
     * 32-bit X11 visuals (the overlay does not work).  Is this a wxWindows
     * bug?  Is this a Gstreamer bug?  No idea, but it is our problem ... 
     * and anyway, this transparency thing just isn't all that interesting,
     * so we just don't do it on Linux. 
     */
#ifndef __WXGTK__
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
#endif
}
