/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/misc.c: Miscellaneous interface and library functions
 * for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* SYSERR_* error code for the last failing system function. */
static int last_error_code;

/* Corresponding Windows error code, or 0 if none. */
static DWORD last_windows_error_code;

/*************************************************************************/
/************************** Interface functions **************************/
/*************************************************************************/

void sys_console_vprintf(const char *format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    const int len = vstrformat(NULL, 0, format, args_copy);
    va_end(args_copy);

    char *buf = mem_alloc(len+1, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!buf)) {
        DLOG("No memory for console output string (%d bytes)", len+1);
        return;
    }
    ASSERT(vstrformat(buf, len+1, format, args) == len);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, len, (DWORD[1]){0}, NULL);
    mem_free(buf);
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_NOISY_ERRORS
# define USED_IF_NOISY  /*nothing*/
#else
# define USED_IF_NOISY  UNUSED
#endif

void sys_display_error(USED_IF_NOISY const char *message,
                       USED_IF_NOISY va_list args)
{
#ifdef SIL_UTILITY_NOISY_ERRORS
    char buf[1000];
    vstrformat(buf, sizeof(buf), message, args);
    const char *title = windows_window_title();
    windows_show_mouse_pointer(1);
    MessageBox(NULL, buf, title && *title ? title : "Error", MB_ICONERROR);
    windows_show_mouse_pointer(-1);
#endif
}

#undef USED_IF_NOISY

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    /* Mapping of Windows language codes to language/dialect pairs.  Any
     * entry with SUBLANG_NEUTRAL matches all language IDs with the same
     * primary language code. */
    static const struct {LANGID id; char language[2], dialect[2];} map[] = {
        {MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),             "en", "US"},
        {MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL),           "en", "US"},

        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_ALGERIA),       "ar", "DZ"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_BAHRAIN),       "ar", "BH"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_EGYPT),         "ar", "EG"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_IRAQ),          "ar", "IQ"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_JORDAN),        "ar", "JO"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_KUWAIT),        "ar", "KW"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_LEBANON),       "ar", "LB"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_LIBYA),         "ar", "LY"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_MOROCCO),       "ar", "MA"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_OMAN),          "ar", "OM"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_QATAR),         "ar", "QA"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_SAUDI_ARABIA),  "ar", "SA"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_SYRIA),         "ar", "SY"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_TUNISIA),       "ar", "TN"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_UAE),           "ar", "AE"},
        {MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_YEMEN),         "ar", "YE"},

#ifdef SUBLANG_BENGALI_BANGLADESH
        {MAKELANGID(LANG_BENGALI, SUBLANG_BENGALI_BANGLADESH),  "bn", "BD"},
#endif
#ifdef SUBLANG_BENGALI_INDIA
        {MAKELANGID(LANG_BENGALI, SUBLANG_BENGALI_INDIA),       "bn", "IN"},
#endif

        {MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_HONGKONG),    "zh", "HK"},
        {MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_MACAU),       "zh", "MO"},
        {MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),  "zh", "CN"},
        {MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SINGAPORE),   "zh", "SG"},
        {MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL), "zh", "TW"},

        {MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH),                 "nl", "NL"},
        {MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH_BELGIAN),         "nl", "BE"},

        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_AUS),         "en", "AU"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_BELIZE),      "en", "BZ"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_CAN),         "en", "CA"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_CARIBBEAN),   "en", ""},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_EIRE),        "en", "IE"},
#ifdef SUBLANG_ENGLISH_INDIA
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_INDIA),       "en", "IN"},
#endif
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_JAMAICA),     "en", "JM"},
#ifdef SUBLANG_ENGLISH_MALAYSIA
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_MALAYSIA),    "en", "MY"},
#endif
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_NZ),          "en", "NZ"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_PHILIPPINES), "en", "PH"},
#ifdef SUBLANG_ENGLISH_SINGAPORE
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_SINGAPORE),   "en", "SG"},
#endif
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA),"en", "ZA"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_TRINIDAD),    "en", "TT"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_UK),          "en", "GB"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),          "en", "US"},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_ZIMBABWE),    "en", "ZW"},

        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),               "fr", "FR"},
        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_BELGIAN),       "fr", "BE"},
        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_CANADIAN),      "fr", "CA"},
        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_LUXEMBOURG),    "fr", "LU"},
        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_MONACO),        "fr", "MC"},
        {MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_SWISS),         "fr", "CH"},

        {MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN),               "de", "DE"},
        {MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN),      "de", "AT"},
        {MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_LIECHTENSTEIN), "de", "LI"},
        {MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_LUXEMBOURG),    "de", "LU"},
        {MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_SWISS),         "de", "CH"},

        {MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN),             "it", "IT"},
        {MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN_SWISS),       "it", "CH"},

        {MAKELANGID(LANG_MALAY, SUBLANG_MALAY_BRUNEI_DARUSSALAM),"ms", "BN"},
        {MAKELANGID(LANG_MALAY, SUBLANG_MALAY_MALAYSIA),        "ms", "MY"},

#ifdef SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA
        {MAKELANGID(LANG_MONGOLIAN, SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA),
                                                                "mn", "MN"},
#endif
#ifdef SUBLANG_MONGOLIAN_PRC
        {MAKELANGID(LANG_MONGOLIAN, SUBLANG_MONGOLIAN_PRC),     "mn", "ZH"},
#endif

        {MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL),  "nb", "NO"},
        {MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK), "nn", "NO"},

        {MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE),       "pt", "PT"},
        {MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN),"pt", "BR"},

#ifdef SUBLANG_QUECHUA_BOLIVIA
        {MAKELANGID(LANG_QUECHUA, SUBLANG_QUECHUA_BOLIVIA),     "qu", "BO"},
#endif
#ifdef SUBLANG_QUECHUA_ECUADOR
        {MAKELANGID(LANG_QUECHUA, SUBLANG_QUECHUA_ECUADOR),     "qu", "EC"},
#endif
#ifdef SUBLANG_QUECHUA_PERU
        {MAKELANGID(LANG_QUECHUA, SUBLANG_QUECHUA_PERU),        "qu", "PE"},
#endif

        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_INARI_FINLAND),     "se", "FI"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_LULE_NORWAY),       "se", "NO"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_LULE_SWEDEN),       "se", "SE"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_NORTHERN_FINLAND),  "se", "FI"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_NORTHERN_NORWAY),   "se", "NO"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_NORTHERN_SWEDEN),   "se", "SE"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_SKOLT_FINLAND),     "se", "FI"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_SOUTHERN_NORWAY),   "se", "NO"},
        {MAKELANGID(LANG_SAMI, SUBLANG_SAMI_SOUTHERN_SWEDEN),   "se", "SE"},

#ifdef SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_CYRILLIC
        {MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_CYRILLIC),
                                                                "sr", "BA"},
        {MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_BOSNIA_HERZEGOVINA_LATIN),
                                                                "sr", "BA"},
#endif
        {MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC),    "sr", "RS"},
        {MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN),       "sr", "RS"},

        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH),             "es", "ES"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_ARGENTINA),   "es", "AR"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_BOLIVIA),     "es", "BO"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_CHILE),       "es", "CL"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_COLOMBIA),    "es", "CO"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_COSTA_RICA),  "es", "CR"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_DOMINICAN_REPUBLIC),
                                                                "es", "DO"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_ECUADOR),     "es", "EC"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_EL_SALVADOR), "es", "SV"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_GUATEMALA),   "es", "GT"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_HONDURAS),    "es", "HN"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MEXICAN),     "es", "MX"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_NICARAGUA),   "es", "NI"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_PANAMA),      "es", "PA"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_PARAGUAY),    "es", "PY"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_PERU),        "es", "PE"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_PUERTO_RICO), "es", "PR"},
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_URUGUAY),     "es", "UY"},
#ifdef SUBLANG_SPANISH_US
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_US),          "es", "US"},
#endif
        {MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_VENEZUELA),   "es", "VE"},

        {MAKELANGID(LANG_SWEDISH, SUBLANG_SWEDISH),             "sv", "SE"},
        {MAKELANGID(LANG_SWEDISH, SUBLANG_SWEDISH_FINLAND),     "sv", "FI"},

        {MAKELANGID(LANG_URDU, SUBLANG_URDU_PAKISTAN),          "ur", "PK"},
        {MAKELANGID(LANG_URDU, SUBLANG_URDU_INDIA),             "ur", "IN"},

        /* These are fallback matches for languages with unknown or default
         * sublanguage codes, and must come at the end of the table. */
        {MAKELANGID(LANG_AFRIKAANS, SUBLANG_NEUTRAL),     "af", ""},
        {MAKELANGID(LANG_ALBANIAN, SUBLANG_NEUTRAL),      "sq", ""},
        {MAKELANGID(LANG_ALSATIAN, SUBLANG_NEUTRAL),      "??", ""},  // gsw
#ifdef LANG_AMHARIC
        {MAKELANGID(LANG_AMHARIC, SUBLANG_NEUTRAL),       "am", ""},
#endif
        {MAKELANGID(LANG_ARABIC, SUBLANG_NEUTRAL),        "ar", ""},
        {MAKELANGID(LANG_ARMENIAN, SUBLANG_NEUTRAL),      "hy", ""},
#ifdef LANG_ASSAMESE
        {MAKELANGID(LANG_ASSAMESE, SUBLANG_NEUTRAL),      "as", ""},
#endif
        {MAKELANGID(LANG_AZERI, SUBLANG_NEUTRAL),         "az", ""},
        {MAKELANGID(LANG_BASHKIR, SUBLANG_NEUTRAL),       "ba", ""},
        {MAKELANGID(LANG_BASQUE, SUBLANG_NEUTRAL),        "eu", ""},
        {MAKELANGID(LANG_BELARUSIAN, SUBLANG_NEUTRAL),    "be", ""},
        {MAKELANGID(LANG_BENGALI, SUBLANG_NEUTRAL),       "bn", ""},
        {MAKELANGID(LANG_BRETON, SUBLANG_NEUTRAL),        "br", ""},
        {MAKELANGID(LANG_BULGARIAN, SUBLANG_NEUTRAL),     "bg", ""},
        {MAKELANGID(LANG_CATALAN, SUBLANG_NEUTRAL),       "ca", ""},
        {MAKELANGID(LANG_CHINESE, SUBLANG_NEUTRAL),       "zh", ""},
        {MAKELANGID(LANG_CORSICAN, SUBLANG_NEUTRAL),      "co", ""},
        {MAKELANGID(LANG_CROATIAN, SUBLANG_NEUTRAL),      "hr", ""},
        {MAKELANGID(LANG_CZECH, SUBLANG_NEUTRAL),         "cs", ""},
        {MAKELANGID(LANG_DANISH, SUBLANG_NEUTRAL),        "da", ""},
        {MAKELANGID(LANG_DARI, SUBLANG_NEUTRAL),          "??", ""},
        {MAKELANGID(LANG_DIVEHI, SUBLANG_NEUTRAL),        "dv", ""},
        {MAKELANGID(LANG_DUTCH, SUBLANG_NEUTRAL),         "nl", ""},
        {MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL),       "en", ""},
        {MAKELANGID(LANG_ESTONIAN, SUBLANG_NEUTRAL),      "et", ""},
        {MAKELANGID(LANG_FAEROESE, SUBLANG_NEUTRAL),      "fo", ""},
        {MAKELANGID(LANG_FARSI, SUBLANG_NEUTRAL),         "fa", ""},
#ifdef LANG_FILIPINO
        {MAKELANGID(LANG_FILIPINO, SUBLANG_NEUTRAL),      "??", ""},  // fil
#endif
        {MAKELANGID(LANG_FINNISH, SUBLANG_NEUTRAL),       "fi", ""},
        {MAKELANGID(LANG_FRENCH, SUBLANG_NEUTRAL),        "fr", ""},
#ifdef LANG_FRISIAN
        {MAKELANGID(LANG_FRISIAN, SUBLANG_NEUTRAL),       "fy", ""},
#endif
        {MAKELANGID(LANG_GALICIAN, SUBLANG_NEUTRAL),      "gl", ""},
        {MAKELANGID(LANG_GEORGIAN, SUBLANG_NEUTRAL),      "ka", ""},
        {MAKELANGID(LANG_GERMAN, SUBLANG_NEUTRAL),        "de", ""},
        {MAKELANGID(LANG_GREEK, SUBLANG_NEUTRAL),         "el", ""},
        {MAKELANGID(LANG_GREENLANDIC, SUBLANG_NEUTRAL),   "kl", ""},
        {MAKELANGID(LANG_GUJARATI, SUBLANG_NEUTRAL),      "gu", ""},
        {MAKELANGID(LANG_HAUSA, SUBLANG_NEUTRAL),         "ha", ""},
        {MAKELANGID(LANG_HEBREW, SUBLANG_NEUTRAL),        "he", ""},
        {MAKELANGID(LANG_HINDI, SUBLANG_NEUTRAL),         "hi", ""},
        {MAKELANGID(LANG_HUNGARIAN, SUBLANG_NEUTRAL),     "hu", ""},
        {MAKELANGID(LANG_ICELANDIC, SUBLANG_NEUTRAL),     "is", ""},
        {MAKELANGID(LANG_IGBO, SUBLANG_NEUTRAL),          "ig", ""},
        {MAKELANGID(LANG_INDONESIAN, SUBLANG_NEUTRAL),    "id", ""},
#ifdef LANG_INUKTITUT
        {MAKELANGID(LANG_INUKTITUT, SUBLANG_NEUTRAL),     "iu", ""},
#endif
#ifdef LANG_IRISH
        {MAKELANGID(LANG_IRISH, SUBLANG_NEUTRAL),         "ga", ""},
#endif
        {MAKELANGID(LANG_ITALIAN, SUBLANG_NEUTRAL),       "it", ""},
        {MAKELANGID(LANG_JAPANESE, SUBLANG_NEUTRAL),      "ja", ""},
        {MAKELANGID(LANG_KANNADA, SUBLANG_NEUTRAL),       "kn", ""},
        {MAKELANGID(LANG_KASHMIRI, SUBLANG_NEUTRAL),      "ks", ""},
        {MAKELANGID(LANG_KAZAK, SUBLANG_NEUTRAL),         "kk", ""},
#ifdef LANG_KHMER
        {MAKELANGID(LANG_KHMER, SUBLANG_NEUTRAL),         "km", ""},
#endif
        {MAKELANGID(LANG_KICHE, SUBLANG_NEUTRAL),         "??", ""},
        {MAKELANGID(LANG_KINYARWANDA, SUBLANG_NEUTRAL),   "rw", ""},
        {MAKELANGID(LANG_KONKANI, SUBLANG_NEUTRAL),       "??", ""},  // kok
        {MAKELANGID(LANG_KOREAN, SUBLANG_NEUTRAL),        "ko", ""},
        {MAKELANGID(LANG_KYRGYZ, SUBLANG_NEUTRAL),        "ky", ""},
#ifdef LANG_LAO
        {MAKELANGID(LANG_LAO, SUBLANG_NEUTRAL),           "lo", ""},
#endif
        {MAKELANGID(LANG_LATVIAN, SUBLANG_NEUTRAL),       "lv", ""},
        {MAKELANGID(LANG_LITHUANIAN, SUBLANG_NEUTRAL),    "lt", ""},
#ifdef LANG_LOWER_SORBIAN
        {MAKELANGID(LANG_LOWER_SORBIAN, SUBLANG_NEUTRAL), "??", ""},  // dsb
#endif
        {MAKELANGID(LANG_LUXEMBOURGISH, SUBLANG_NEUTRAL), "lb", ""},
        {MAKELANGID(LANG_MACEDONIAN, SUBLANG_NEUTRAL),    "mk", ""},
#ifdef LANG_MALAGASY
        {MAKELANGID(LANG_MALAGASY, SUBLANG_NEUTRAL),      "mg", ""},
#endif
        {MAKELANGID(LANG_MALAY, SUBLANG_NEUTRAL),         "ms", ""},
        {MAKELANGID(LANG_MALAYALAM, SUBLANG_NEUTRAL),     "ml", ""},
#ifdef LANG_MALTESE
        {MAKELANGID(LANG_MALTESE, SUBLANG_NEUTRAL),       "mt", ""},
#endif
        {MAKELANGID(LANG_MANIPURI, SUBLANG_NEUTRAL),      "??", ""},  // mni
        {MAKELANGID(LANG_MAORI, SUBLANG_NEUTRAL),         "mi", ""},
        {MAKELANGID(LANG_MAPUDUNGUN, SUBLANG_NEUTRAL),    "am", ""},
        {MAKELANGID(LANG_MARATHI, SUBLANG_NEUTRAL),       "mr", ""},
        {MAKELANGID(LANG_MOHAWK, SUBLANG_NEUTRAL),        "??", ""},  // moh
        {MAKELANGID(LANG_MONGOLIAN, SUBLANG_NEUTRAL),     "mn", ""},
        {MAKELANGID(LANG_NEPALI, SUBLANG_NEUTRAL),        "ne", ""},
        {MAKELANGID(LANG_NORWEGIAN, SUBLANG_NEUTRAL),     "nb", ""},
        {MAKELANGID(LANG_OCCITAN, SUBLANG_NEUTRAL),       "oc", ""},
        {MAKELANGID(LANG_ORIYA, SUBLANG_NEUTRAL),         "or", ""},
#ifdef LANG_PASHTO
        {MAKELANGID(LANG_PASHTO, SUBLANG_NEUTRAL),        "ps", ""},
#endif
        {MAKELANGID(LANG_POLISH, SUBLANG_NEUTRAL),        "pl", ""},
        {MAKELANGID(LANG_PORTUGUESE, SUBLANG_NEUTRAL),    "pt", ""},
        {MAKELANGID(LANG_PUNJABI, SUBLANG_NEUTRAL),       "pa", ""},
        {MAKELANGID(LANG_QUECHUA, SUBLANG_NEUTRAL),       "qu", ""},
        {MAKELANGID(LANG_ROMANIAN, SUBLANG_NEUTRAL),      "ro", ""},
        {MAKELANGID(LANG_RUSSIAN, SUBLANG_NEUTRAL),       "ru", ""},
#ifdef LANG_SAMI
        {MAKELANGID(LANG_SAMI, SUBLANG_NEUTRAL),          "se", ""},
#endif
        {MAKELANGID(LANG_SANSKRIT, SUBLANG_NEUTRAL),      "sa", ""},
        {MAKELANGID(LANG_SERBIAN, SUBLANG_NEUTRAL),       "sr", ""},
        {MAKELANGID(LANG_SINDHI, SUBLANG_NEUTRAL),        "sd", ""},
#ifdef LANG_SINHALESE
        {MAKELANGID(LANG_SINHALESE, SUBLANG_NEUTRAL),     "si", ""},
#endif
        {MAKELANGID(LANG_SLOVAK, SUBLANG_NEUTRAL),        "sk", ""},
        {MAKELANGID(LANG_SLOVENIAN, SUBLANG_NEUTRAL),     "sl", ""},
        {MAKELANGID(LANG_SOTHO, SUBLANG_NEUTRAL),         "st", ""},
        {MAKELANGID(LANG_SPANISH, SUBLANG_NEUTRAL),       "es", ""},
        {MAKELANGID(LANG_SWAHILI, SUBLANG_NEUTRAL),       "sw", ""},
        {MAKELANGID(LANG_SWEDISH, SUBLANG_NEUTRAL),       "sv", ""},
        {MAKELANGID(LANG_SYRIAC, SUBLANG_NEUTRAL),        "??", ""},  // syr
#ifdef LANG_TAMAZIGHT
        {MAKELANGID(LANG_TAMAZIGHT, SUBLANG_NEUTRAL),     "??", ""},
#endif
        {MAKELANGID(LANG_TAMIL, SUBLANG_NEUTRAL),         "ta", ""},
        {MAKELANGID(LANG_TATAR, SUBLANG_NEUTRAL),         "tt", ""},
        {MAKELANGID(LANG_TELUGU, SUBLANG_NEUTRAL),        "te", ""},
        {MAKELANGID(LANG_THAI, SUBLANG_NEUTRAL),          "th", ""},
#ifdef LANG_TIBETAN
        {MAKELANGID(LANG_TIBETAN, SUBLANG_NEUTRAL),       "bo", ""},
#endif
        {MAKELANGID(LANG_TIGRIGNA, SUBLANG_NEUTRAL),      "ti", ""},
#ifdef LANG_TSWANA
        {MAKELANGID(LANG_TSWANA, SUBLANG_NEUTRAL),        "tn", ""},
#endif
        {MAKELANGID(LANG_TURKISH, SUBLANG_NEUTRAL),       "tr", ""},
        {MAKELANGID(LANG_UIGHUR, SUBLANG_NEUTRAL),        "ug", ""},
        {MAKELANGID(LANG_UKRAINIAN, SUBLANG_NEUTRAL),     "uk", ""},
#ifdef LANG_UPPER_SORBIAN
        {MAKELANGID(LANG_UPPER_SORBIAN, SUBLANG_NEUTRAL), "??", ""},  // hsb
#endif
        {MAKELANGID(LANG_URDU, SUBLANG_NEUTRAL),          "ur", ""},
        {MAKELANGID(LANG_UZBEK, SUBLANG_NEUTRAL),         "uz", ""},
        {MAKELANGID(LANG_VIETNAMESE, SUBLANG_NEUTRAL),    "vi", ""},
#ifdef LANG_WELSH
        {MAKELANGID(LANG_WELSH, SUBLANG_NEUTRAL),         "dy", ""},
#endif
        {MAKELANGID(LANG_WOLOF, SUBLANG_NEUTRAL),         "wo", ""},
#ifdef LANG_XHOSA
        {MAKELANGID(LANG_XHOSA, SUBLANG_NEUTRAL),         "xh", ""},
#endif
        {MAKELANGID(LANG_YAKUT, SUBLANG_NEUTRAL),         "??", ""},  // sah
        {MAKELANGID(LANG_YI, SUBLANG_NEUTRAL),            "??", ""},
        {MAKELANGID(LANG_YORUBA, SUBLANG_NEUTRAL),        "yo", ""},
#ifdef LANG_ZULU
        {MAKELANGID(LANG_ZULU, SUBLANG_NEUTRAL),          "zu", ""},
#endif
    };

    LANGID langid;
    if (index == 0) {
        langid = GetUserDefaultUILanguage();
    } else if (index == 1) {
        langid = GetSystemDefaultUILanguage();
    } else {
        return 0;
    }

    for (int i = 0; i < lenof(map); i++) {
        if (map[i].id == langid
            || (SUBLANGID(map[i].id) == SUBLANG_NEUTRAL
                && PRIMARYLANGID(map[i].id) == PRIMARYLANGID(langid)))
        {
            language_ret[0] = map[i].language[0];
            language_ret[1] = map[i].language[1];
            language_ret[2] = 0;
            dialect_ret[0] = map[i].dialect[0];
            dialect_ret[1] = map[i].dialect[1];
            dialect_ret[2] = 0;
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_get_resource_path_prefix(char *prefix_buf, int bufsize)
{
    const char *path = windows_executable_dir();
#ifdef SIL_DATA_PATH_ENV_VAR
    const char *env_path = getenv(SIL_DATA_PATH_ENV_VAR);
    if (env_path && *env_path) {
        path = env_path;
    }
#endif
    return strformat(prefix_buf, bufsize, "%s/", path);
}

/*-----------------------------------------------------------------------*/

int sys_last_error(void)
{
    return last_error_code;
}

/*-----------------------------------------------------------------------*/

const char *sys_last_errstr(void)
{
    switch (last_error_code) {
      case SYSERR_FILE_NOT_FOUND:
        return "File not found";
      case SYSERR_FILE_ACCESS_DENIED:
        return "Access denied";
      case SYSERR_FILE_ASYNC_ABORTED:
        return "Asynchronous read aborted";
      case SYSERR_FILE_ASYNC_INVALID:
        return "Invalid asynchronous read ID";
      case SYSERR_FILE_ASYNC_FULL:
        return "Asynchronous read table full";
      default:
        if (last_windows_error_code) {
            return windows_strerror(last_windows_error_code);
        } else {
            return "Unknown error";
        }
    }
}

/*-----------------------------------------------------------------------*/

int sys_open_file(const char *path)
{
    if (!path) {
        return 1;
    }

    const int result = (intptr_t)ShellExecute(NULL, "open", path, NULL, NULL,
                                              SW_SHOWNORMAL);
    if (result > 32) {  // Magic number that indicates success.
        return 1;
    } else if (result == 0) {
        DLOG("%s: Out of resources", path);
        last_error_code = SYSERR_UNKNOWN_ERROR;
        return 0;
    } else {
        DLOG("%s: %s", path, windows_strerror(result));
        windows_set_error(0, result);
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    /* ShellExecute() also works for URLs (says KB224816). */
    return sys_open_file(url);
}

/*-----------------------------------------------------------------------*/

uint64_t sys_random_seed(void)
{
    SYSTEMTIME time;
    GetSystemTime(&time);
    FILETIME time2;
    SystemTimeToFileTime(&time, &time2);
    return ((uint64_t)time2.dwHighDateTime << 32) | time2.dwLowDateTime;
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    SetThreadExecutionState(ES_DISPLAY_REQUIRED);
}

/*-----------------------------------------------------------------------*/

int sys_set_performance_level(int level)
{
    return level == 0;  // Alternate performance levels not supported.
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

void windows_set_error(int code, DWORD windows_code)
{
    if (code) {
        last_error_code = code;
        last_windows_error_code = 0;
    } else {
        if (!windows_code) {
            windows_code = GetLastError();
        }
        if (windows_code == ERROR_INVALID_HANDLE) {
            last_error_code = SYSERR_INVALID_PARAMETER;
        } else if (windows_code == ERROR_NOT_ENOUGH_MEMORY
                || windows_code == ERROR_OUTOFMEMORY) {
            last_error_code = SYSERR_OUT_OF_MEMORY;
        } else if (windows_code == ERROR_FILE_NOT_FOUND
                || windows_code == ERROR_PATH_NOT_FOUND) {
            last_error_code = SYSERR_FILE_NOT_FOUND;
        } else if (windows_code == ERROR_ACCESS_DENIED) {
            last_error_code = SYSERR_FILE_ACCESS_DENIED;
        } else if (windows_code == ERROR_OPERATION_ABORTED) {
            last_error_code = SYSERR_FILE_ASYNC_ABORTED;
        } else {
            last_error_code = SYSERR_UNKNOWN_ERROR;
        }
        last_windows_error_code = code;
    }
}

/*************************************************************************/
/*************************************************************************/
