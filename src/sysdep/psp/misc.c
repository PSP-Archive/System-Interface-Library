/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/misc.c: Miscellaneous PSP-specific routines.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/utility/misc.h"

/*************************************************************************/
/********************** External interface routines **********************/
/*************************************************************************/

void sys_console_vprintf(const char *format, va_list args)
{
    char buf[1000];
    unsigned int len = vstrformat(buf, sizeof(buf), format, args);
    sceIoWrite(1, buf, len);
}

/*-----------------------------------------------------------------------*/

void sys_display_error(const char *message, va_list args)
{
    char buf[1000];
    memcpy(buf, "Error: ", 7);
    unsigned int len = 7 + vstrformat(buf+7, sizeof(buf)-7, message, args);
    if (len < sizeof(buf)) {
        buf[len++] = '\n';
    }
    sceIoWrite(2, buf, len);
}

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    static const struct {
        uint8_t code;
        char language[3];
        char dialect[3];
    } languages[] = {
        {PSP_SYSTEMPARAM_LANGUAGE_JAPANESE,   "ja", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_ENGLISH,    "en", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_FRENCH,     "fr", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_SPANISH,    "es", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_GERMAN,     "de", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_ITALIAN,    "it", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_DUTCH,      "nl", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE, "pt", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN,    "ru", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_KOREAN,     "ko", ""},
        {PSP_SYSTEMPARAM_LANGUAGE_CHINESE_TRADITIONAL, "zh", "TW"},
        {PSP_SYSTEMPARAM_LANGUAGE_CHINESE_SIMPLIFIED,  "zh", "CN"},
    };

    if (index == 0) {
        int language;
        const int result = sceUtilityGetSystemParamInt(
            PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &language);
        if (UNLIKELY(result != 0)) {
            static uint8_t warned = 0;
            if (!warned) {
                DLOG("sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT"
                     "_LANGUAGE): %s", psp_strerror(result));
                warned = 1;
            }
            return 0;
        }
        for (int i = 0; i < lenof(languages); i++) {
            if (language == languages[i].code) {
                memcpy(language_ret, languages[i].language, 3);
                memcpy(dialect_ret, languages[i].dialect, 3);
                return 1;
            }
        }
        static uint8_t warned = 0;
        if (!warned) {
            DLOG("Unknown language code: %d", language);
            warned = 1;
        }
        return 0;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_get_resource_path_prefix(char *prefix_buf, int bufsize)
{
    return strformat(prefix_buf, bufsize, "%s/", psp_executable_dir());
}

/*-----------------------------------------------------------------------*/

int sys_last_error(void)
{
    switch (psp_errno) {
        case PSP_EINVAL:       return SYSERR_INVALID_PARAMETER;
        case PSP_ENOMEM:       return SYSERR_OUT_OF_MEMORY;
        case PSP_EMFILE:       return SYSERR_OUT_OF_MEMORY;
        case PSP_ENAMETOOLONG: return SYSERR_BUFFER_OVERFLOW;
        case PSP_EAGAIN:       return SYSERR_TRANSIENT_FAILURE;
        case PSP_ENOENT:       return SYSERR_FILE_NOT_FOUND;
        case PSP_EACCES:       return SYSERR_FILE_ACCESS_DENIED;
        case PSP_EISDIR:       return SYSERR_FILE_WRONG_TYPE;
        case PSP_ENOTDIR:      return SYSERR_FILE_WRONG_TYPE;
        case PSP_ECANCELED:    return SYSERR_FILE_ASYNC_ABORTED;
        case PSP_ENOEXEC:      return SYSERR_FILE_ASYNC_FULL;  // See files.c.
        case SCE_KERNEL_ERROR_INVAL:   return SYSERR_INVALID_PARAMETER;
        case SCE_KERNEL_ERROR_MFILE:   return SYSERR_OUT_OF_MEMORY;
        case SCE_KERNEL_ERROR_NOASYNC: return SYSERR_FILE_ASYNC_INVALID;
    }
    return SYSERR_UNKNOWN_ERROR;
}

/*-----------------------------------------------------------------------*/

const char *sys_last_errstr(void)
{
    if (psp_errno == PSP_ENOEXEC) {
        return "Asynchronous read table full";
    } else {
        return psp_strerror(psp_errno);
    }
}

/*-----------------------------------------------------------------------*/

int sys_open_file(UNUSED const char *path)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

int sys_open_url(UNUSED const char *url)
{
    return 0;  // Not supported on the PSP.
}

/*-----------------------------------------------------------------------*/

uint64_t sys_random_seed(void)
{
    return sceKernelGetSystemTimeWide();
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    scePowerTick(0);
}

/*-----------------------------------------------------------------------*/

int sys_set_performance_level(int level)
{
    switch (level) {
      case PERFORMANCE_LEVEL_LOW:
        level = 111;
        break;
      case PERFORMANCE_LEVEL_DEFAULT:
        level = 222;
        break;
      case PERFORMANCE_LEVEL_HIGH:
        level = 333;
        break;
      default:
        if (level < 2 || level > 333) {
            DLOG("CPU frequency out of range: %d MHz", level);
            return 0;
        }
    }
    scePowerSetClockFrequency(level, level, level/2);
    return 1;
}

/*************************************************************************/
/************************* Internal-use routines *************************/
/*************************************************************************/

const char *psp_strerror(uint32_t code)
{
    static const struct {
        uint32_t code;
        const char *message;
    } errors[] = {
        {0x80000023,  "Invalid address"},
        {PSP_EPERM,   "Operation not permitted"},
        {PSP_ENOENT,  "No such file or directory"},
        {PSP_ESRCH,   "No such process"},
        {PSP_EINTR,   "Interrupted system call"},
        {PSP_EIO,     "I/O error"},
        {PSP_ENXIO,   "No such device or address"},
        {PSP_E2BIG,   "Argument list too long"},
        {PSP_ENOEXEC, "Asynchronous read table full"},  // See files.c.
        {PSP_EBADF,   "Bad file number"},
        {PSP_ECHILD,  "No child processes"},
        {PSP_EAGAIN,  "Try again"},
        {PSP_ENOMEM,  "Out of memory"},
        {PSP_EACCES,  "Permission denied"},
        {PSP_EFAULT,  "Bad address"},
        {PSP_ENOTBLK, "Block device required"},
        {PSP_EBUSY,   "Device or resource busy"},
        {PSP_EEXIST,  "File exists"},
        {PSP_EXDEV,   "Cross-device link"},
        {PSP_ENODEV,  "No such device"},
        {PSP_ENOTDIR, "Not a directory"},
        {PSP_EISDIR,  "Is a directory"},
        {PSP_EINVAL,  "Invalid argument"},
        {PSP_ENFILE,  "File table overflow"},
        {PSP_EMFILE,  "Too many open files"},
        {PSP_ENOTTY,  "Not a typewriter"},
        {PSP_ETXTBSY, "Text file busy"},
        {PSP_EFBIG,   "File too large"},
        {PSP_ENOSPC,  "No space left on device"},
        {PSP_ESPIPE,  "Illegal seek"},
        {PSP_EROFS,   "Read-only file system"},
        {PSP_EMLINK,  "Too many links"},
        {PSP_EPIPE,   "Broken pipe"},
        {PSP_EDOM,    "Math argument out of domain of func"},
        {PSP_ERANGE,  "Math result not representable"},
        {PSP_EDEADLK, "Resource deadlock would occur"},
        {PSP_ENAMETOOLONG,             "File name too long"},
        {PSP_ECANCELED,                "Operation cancelled"},
        {SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT,   "Invalid argument"},
        {SCE_KERNEL_ERROR_ILLEGAL_ADDR,       "Bad address"},
        {SCE_KERNEL_ERROR_NOFILE,             "File not found"},
        {SCE_KERNEL_ERROR_NO_MEMORY,          "Out of memory"},
        {SCE_KERNEL_ERROR_ILLEGAL_ATTR,       "Invalid attribute"},
        {SCE_KERNEL_ERROR_ILLEGAL_ENTRY,      "Invalid entry point"},
        {SCE_KERNEL_ERROR_ILLEGAL_PRIORITY,   "Invalid priority"},
        {SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE, "Invalid stack size"},
        {SCE_KERNEL_ERROR_ILLEGAL_MODE,       "Invalid mode"},
        {SCE_KERNEL_ERROR_ILLEGAL_MASK,       "Invalid mask"},
        {SCE_KERNEL_ERROR_ILLEGAL_THID,       "Invalid thread ID"},
        {SCE_KERNEL_ERROR_UNKNOWN_THID,       "Unknown thread ID"},
        {SCE_KERNEL_ERROR_UNKNOWN_SEMID,      "Unknown semaphore ID"},
        {SCE_KERNEL_ERROR_UNKNOWN_EVFID,      "Unknown event flag ID"},
        {SCE_KERNEL_ERROR_UNKNOWN_MBXID,      "Unknown mailbox ID"},
        {SCE_KERNEL_ERROR_MFILE,       "Too many files open"},
        {SCE_KERNEL_ERROR_NODEV,       "Device not found"},
        {SCE_KERNEL_ERROR_XDEV,        "Cross-device link"},
        {SCE_KERNEL_ERROR_INVAL,       "Invalid argument"},
        {SCE_KERNEL_ERROR_BADF,        "Bad file descriptor"},
        {SCE_KERNEL_ERROR_NAMETOOLONG, "File name too long"},
        {SCE_KERNEL_ERROR_IO,          "I/O error"},
        {SCE_KERNEL_ERROR_NOMEM,       "Out of memory"},
        {SCE_KERNEL_ERROR_ASYNC_BUSY,  "Asynchronous I/O in progress"},
        {SCE_KERNEL_ERROR_NOASYNC,     "No asynchronous I/O in progress"},
        {PSP_UTILITY_BAD_ADDRESS,      "sceUtility: Bad address"},
        {PSP_UTILITY_BAD_PARAM_SIZE,   "sceUtility: Invalid parameter size"},
        {PSP_UTILITY_BUSY,             "sceUtility: Other utility busy"},
        {PSP_SAVEDATA_LOAD_NO_CARD,    "sceUtilitySavedata: No memory card inserted (load)"},
        {PSP_SAVEDATA_LOAD_IO_ERROR,   "sceUtilitySavedata: I/O error (load)"},
        {PSP_SAVEDATA_LOAD_CORRUPT,    "sceUtilitySavedata: Save file corrupt"},
        {PSP_SAVEDATA_LOAD_NOT_FOUND,  "sceUtilitySavedata: Save file not found"},
        {PSP_SAVEDATA_LOAD_BAD_PARAMS, "sceUtilitySavedata: Invalid parameters for load"},
        {PSP_SAVEDATA_SAVE_NO_CARD,    "sceUtilitySavedata: No memory card inserted (save)"},
        {PSP_SAVEDATA_SAVE_CARD_FULL,  "sceUtilitySavedata: Memory card full"},
        {PSP_SAVEDATA_SAVE_WRITE_PROT, "sceUtilitySavedata: Memory card write-protected"},
        {PSP_SAVEDATA_SAVE_IO_ERROR,   "sceUtilitySavedata: I/O error (save)"},
        {PSP_SAVEDATA_SAVE_BAD_PARAMS, "sceUtilitySavedata: Invalid parameters for save"},
        {0x80260003, "sceAudio: Bad channel number"},
        {0x80260009, "sceAudio: Channel is playing"},
        {0x8026000B, "sceAudio: Bad volume"},
        {0x806101FE, "sceMpeg: Invalid parameter"},
        {0x80618005, "sceMpeg: Stream already registered or double init"},
        {0x80618006, "sceMpeg: Initialization failed"},
        {0x806201FE, "sceVideocodec: Invalid parameter / internal error"},
        {0x807F0002, "sceAudiocodec: Invalid codec"},
        {0x807F0003, "sceAudiocodec: EDRAM allocation failed"},
        {0x807F00FD, "sceAudiocodec: Decoding failed"},
    };

    const char *str = NULL;
    for (int i = 0; i < lenof(errors); i++) {
        if (code == errors[i].code) {
            str = errors[i].message;
            break;
        }
    }
    static char errbuf[100];
    strformat(errbuf, sizeof(errbuf), "%08X%s%s",
              code, str ? ": " : "", str ? str : "");
    return errbuf;
}

/*************************************************************************/
/*************************************************************************/
