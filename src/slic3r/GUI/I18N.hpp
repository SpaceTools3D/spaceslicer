#include <wx/string.h>
#include <wx/translation.h>
#include <wx/strconv.h>
#include <string>

#include "libslic3r_version.h"
#include <cctype>
#include <cstring>
// The macros below are re-checked on every inclusion, so the function needs its own guard.
#ifndef slic3r_GUI_I18N_brand_
#define slic3r_GUI_I18N_brand_
namespace Slic3r { namespace GUI { namespace I18N {
// SpaceSlicer: the upstream name never shows in the UI. With or without a translation, "OrcaSlicer" / "Orca Slicer"
// becomes the app name in every text; inside paths and file names (…/OrcaSlicer/…, OrcaSlicer.conf) it stays.
inline wxString brand(wxString s)
{
    static const wxString names[] = {wxString("OrcaSlicer"), wxString("Orca Slicer")};
    for (const wxString &from : names) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != wxString::npos) {
            const size_t end = pos + from.length();
            const bool before = pos > 0 && (s[pos - 1] == '/' || s[pos - 1] == '.' || s[pos - 1] == '-' || s[pos - 1] == '_');
            const bool after  = end < s.length() && (s[end] == '/' || s[end] == '_' ||
                                (s[end] == '.' && end + 1 < s.length() && s[end + 1].IsAscii() &&
                                 std::isalnum(static_cast<unsigned char>(s[end + 1].GetValue()))));
            if (before || after) { pos = end; continue; }
            s.replace(pos, from.length(), wxString::FromUTF8(SLIC3R_APP_NAME));
            pos += std::strlen(SLIC3R_APP_NAME);
        }
    }
    return s;
}
} } }
#endif /* slic3r_GUI_I18N_brand_ */
#ifndef _
#define _(s)    	Slic3r::GUI::I18N::translate((s))
#define _L(s)    	Slic3r::GUI::I18N::translate((s))
#define _devL(s)	wxString((s))
#define _omitL(s)   ("")
#define _utf8(s)    Slic3r::GUI::I18N::translate_utf8((s))
#define _u8L(s)     Slic3r::GUI::I18N::translate_utf8((s))
#endif /* _ */

#ifndef _L_CONTEXT
#define _L_CONTEXT(s, ctx) 	 Slic3r::GUI::I18N::translate((s), (ctx))
#define _u8L_CONTEXT(s, ctx) Slic3r::GUI::I18N::translate_utf8((s), (ctx))
#endif /* _ */

#ifndef L
// !!! If you needed to translate some wxString,
// !!! please use _L(string)
// !!! _() - is a standard wxWidgets macro to translate
// !!! L() is used only for marking localizable string 
// !!! It will be used in "xgettext" to create a Locating Message Catalog.
#define L(s) s
#endif /* L */

#ifndef L_CONTEXT
#define L_CONTEXT(s, context) s
#endif /* L */

#ifndef _CHB
//! macro used to localization, return wxScopedCharBuffer
//! With wxConvUTF8 explicitly specify that the source string is already in UTF-8 encoding
#define _CHB(s) Slic3r::GUI::I18N::brand(wxGetTranslation(wxString(s, wxConvUTF8))).utf8_str()
#endif /* _CHB */

#ifndef slic3r_GUI_I18N_hpp_
#define slic3r_GUI_I18N_hpp_

#include <wx/intl.h>
#include <wx/version.h>

namespace Slic3r { namespace GUI { 

namespace I18N {
	inline wxString translate(const char         *s) { return brand(wxGetTranslation(wxString(s, wxConvUTF8))); }
	inline wxString translate(const wchar_t      *s) { return brand(wxGetTranslation(s)); }
	inline wxString translate(const std::string  &s) { return brand(wxGetTranslation(wxString(s.c_str(), wxConvUTF8))); }
	inline wxString translate(const std::wstring &s) { return brand(wxGetTranslation(s.c_str())); }
	inline wxString translate(const wxString     &s) { return brand(wxGetTranslation(s)); }

	inline wxString translate(const char         *s, const char 	    *plural, unsigned int n) { return brand(wxGetTranslation(wxString(s, wxConvUTF8), wxString(plural, wxConvUTF8), n)); }
	inline wxString translate(const wchar_t      *s, const wchar_t	    *plural, unsigned int n) { return brand(wxGetTranslation(s, plural, n)); }
	inline wxString translate(const std::string  &s, const std::string  &plural, unsigned int n) { return brand(wxGetTranslation(wxString(s.c_str(), wxConvUTF8), wxString(plural.c_str(), wxConvUTF8), n)); }
	inline wxString translate(const std::wstring &s, const std::wstring &plural, unsigned int n) { return brand(wxGetTranslation(s.c_str(), plural.c_str(), n)); }
	inline wxString translate(const wxString     &s, const wxString     &plural, unsigned int n) { return brand(wxGetTranslation(s, plural, n)); }

	inline std::string translate_utf8(const char         *s) { return brand(wxGetTranslation(wxString(s, wxConvUTF8))).ToUTF8().data(); }
	inline std::string translate_utf8(const wchar_t      *s) { return brand(wxGetTranslation(s)).ToUTF8().data(); }
	inline std::string translate_utf8(const std::string  &s) { return brand(wxGetTranslation(wxString(s.c_str(), wxConvUTF8))).ToUTF8().data(); }
	inline std::string translate_utf8(const std::wstring &s) { return brand(wxGetTranslation(s.c_str())).ToUTF8().data(); }
	inline std::string translate_utf8(const wxString     &s) { return brand(wxGetTranslation(s)).ToUTF8().data(); }

	inline std::string translate_utf8(const char         *s, const char 	    *plural, unsigned int n) { return translate(s, plural, n).ToUTF8().data(); }
	inline std::string translate_utf8(const wchar_t      *s, const wchar_t	    *plural, unsigned int n) { return translate(s, plural, n).ToUTF8().data(); }
	inline std::string translate_utf8(const std::string  &s, const std::string  &plural, unsigned int n) { return translate(s, plural, n).ToUTF8().data(); }
	inline std::string translate_utf8(const std::wstring &s, const std::wstring &plural, unsigned int n) { return translate(s, plural, n).ToUTF8().data(); }
	inline std::string translate_utf8(const wxString     &s, const wxString     &plural, unsigned int n) { return translate(s, plural, n).ToUTF8().data(); }

#define _wxGetTranslation_ctx(S, CTX) brand(wxGetTranslation((S), wxEmptyString, (CTX)))

	inline wxString translate(const char *s, const char* ctx)         { return _wxGetTranslation_ctx(wxString(s, wxConvUTF8), ctx); }
	inline wxString translate(const wchar_t *s, const char* ctx)      { return _wxGetTranslation_ctx(s, ctx); }
	inline wxString translate(const std::string &s, const char* ctx)  { return _wxGetTranslation_ctx(wxString(s.c_str(), wxConvUTF8), ctx); }
	inline wxString translate(const std::wstring &s, const char* ctx) { return _wxGetTranslation_ctx(s.c_str(), ctx); }
	inline wxString translate(const wxString &s, const char* ctx)     { return _wxGetTranslation_ctx(s, ctx); }

	inline std::string translate_utf8(const char *s, const char* ctx)         { return _wxGetTranslation_ctx(wxString(s, wxConvUTF8), ctx).ToUTF8().data(); }
	inline std::string translate_utf8(const wchar_t *s, const char* ctx)      { return _wxGetTranslation_ctx(s, ctx).ToUTF8().data(); }
	inline std::string translate_utf8(const std::string &s, const char* ctx)  { return _wxGetTranslation_ctx(wxString(s.c_str(), wxConvUTF8), ctx).ToUTF8().data(); }
	inline std::string translate_utf8(const std::wstring &s, const char* ctx) { return _wxGetTranslation_ctx(s.c_str(), ctx).ToUTF8().data(); }
	inline std::string translate_utf8(const wxString &s, const char* ctx)     { return _wxGetTranslation_ctx(s, ctx).ToUTF8().data(); }

#undef _wxGetTranslation_ctx
} // namespace I18N

// Return translated std::string as a wxString
wxString	L_str(const std::string &str);

} // namespace GUI
} // namespace Slic3r

// Macro to function both as a marker for xgettext and to actually perform the translation.
#ifndef _L_PLURAL
#define _L_PLURAL(s, plural, n) Slic3r::GUI::I18N::translate(s, plural, n)
#endif /* L */

#endif /* slic3r_GUI_I18N_hpp_ */
