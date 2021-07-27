/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/utf8-wrappers.c: UTF-8 wrappers for Windows functions
 * which take UTF-16 string parameters.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/*************************************************************************/

BOOL CreateDirectoryU(const char *lpPathName,
                      LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    uint16_t *lpPathNameW = strdup_8to16(lpPathName);
    if (UNLIKELY(!lpPathNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpPathName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = CreateDirectoryW(lpPathNameW, lpSecurityAttributes);
    mem_free(lpPathNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

HANDLE CreateFileU(const char *lpFileName,
                   DWORD dwDesiredAccess,
                   DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE retval = CreateFileW(lpFileNameW, dwDesiredAccess, dwShareMode,
                                lpSecurityAttributes, dwCreationDisposition,
                                dwFlagsAndAttributes, hTemplateFile);
    mem_free(lpFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

HWND CreateWindowU(const char *lpClassName, const char *lpWindowName,
                   DWORD dwStyle, int x, int y, int nWidth, int nHeight,
                   HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                   LPVOID lpParam)
{
    /* Careful!  lpClassName might be an atom. */
    uint16_t *lpClassNameW;
    if ((uintptr_t)lpClassName < 0x10000U) {
        lpClassNameW = NULL;
    } else {
        lpClassNameW = strdup_8to16(lpClassName);
        if (UNLIKELY(!lpClassNameW)) {
            DLOG("Out of memory converting to UTF-16: %s", lpClassName);
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    }
    /* lpWindowName is documented as optional, so allow NULL. */
    uint16_t *lpWindowNameW;
    if (!lpWindowName) {
        lpWindowNameW = NULL;
    } else {
        lpWindowNameW = strdup_8to16(lpWindowName);
        if (UNLIKELY(!lpWindowNameW)) {
            DLOG("Out of memory converting to UTF-16: %s", lpWindowName);
            mem_free(lpClassNameW);
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    }
    HWND retval = CreateWindowW(
        lpClassNameW ? lpClassNameW : (LPCWSTR)lpClassName, lpWindowNameW,
        dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    mem_free(lpWindowNameW);
    mem_free(lpClassNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

LRESULT DefWindowProcU(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

/*-----------------------------------------------------------------------*/

BOOL DeleteFileU(const char *lpFileName)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = DeleteFileW(lpFileNameW);
    mem_free(lpFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

LRESULT DispatchMessageU(const MSG *lpMsg)
{
    return DispatchMessageW(lpMsg);
}

/*-----------------------------------------------------------------------*/

HANDLE FindFirstFileU(const char *lpFileName,
                      LPWIN32_FIND_DATAU lpFindFileData)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return INVALID_HANDLE_VALUE;
    }
    /* This is safe because WIN32_FIND_DATAU is defined so the beginning of
     * the structure matches WIN32_FIND_DATAW. */
    LPWIN32_FIND_DATAW lpFindFileDataW = (LPWIN32_FIND_DATAW)lpFindFileData;
    HANDLE retval = FindFirstFileW(lpFileNameW, lpFindFileDataW);
    mem_free(lpFileNameW);
    if (retval != INVALID_HANDLE_VALUE) {
        convert_16to8(lpFindFileData->cFileNameW, lpFindFileData->cFileName);
        convert_16to8(lpFindFileData->cAlternateFileNameW,
                      lpFindFileData->cAlternateFileName);
    }
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL FindNextFileU(HANDLE hFindFile,
                   LPWIN32_FIND_DATAU lpFindFileData)
{
    LPWIN32_FIND_DATAW lpFindFileDataW = (LPWIN32_FIND_DATAW)lpFindFileData;
    BOOL retval = FindNextFileW(hFindFile, lpFindFileDataW);
    if (retval) {
        convert_16to8(lpFindFileData->cFileNameW, lpFindFileData->cFileName);
        convert_16to8(lpFindFileData->cAlternateFileNameW,
                      lpFindFileData->cAlternateFileName);
    }
    return retval;
}

/*-----------------------------------------------------------------------*/

extern BOOL FreeEnvironmentStringsU(char *lpszEnvironmentBlock)
{
    mem_free(lpszEnvironmentBlock);
    return TRUE;
}

/*-----------------------------------------------------------------------*/

extern DWORD GetCurrentDirectoryU(DWORD nBufferLength, char *lpBuffer)
{
    uint16_t *lpBufferW;
    if (nBufferLength > 0) {
        ASSERT(lpBuffer != NULL,
               SetLastError(ERROR_INVALID_PARAMETER); return 0);
        lpBufferW = mem_alloc(nBufferLength*2, 2, MEM_ALLOC_TEMP);
        if (UNLIKELY(!lpBufferW)) {
            DLOG("Out of memory allocating %d-element UTF-16 buffer",
                 (int)nBufferLength);
            SetLastError(ERROR_OUTOFMEMORY);
            return 0;
        }
    } else {
        lpBufferW = NULL;
    }
    DWORD retval = GetCurrentDirectoryW(nBufferLength, lpBufferW);
    if (retval == 0) {
        mem_free(lpBufferW);
        return 0;
    }
    if (retval > nBufferLength) {
        /* We need to return the final UTF-8 size, so we need to retry the
         * get with a sufficiently large buffer. */
        mem_free(lpBufferW);
        lpBufferW = mem_alloc(retval*2, 2, MEM_ALLOC_TEMP);
        if (UNLIKELY(!lpBufferW)) {
            DLOG("Out of memory allocating %d-element UTF-16 buffer",
                 (int)retval);
            SetLastError(ERROR_OUTOFMEMORY);
            return 0;
        }
        DWORD retval2 = GetCurrentDirectoryW(retval, lpBufferW);
        ASSERT(retval2 == retval - 1,
               SetLastError(ERROR_BAD_ENVIRONMENT); return 0);
    }
    char *tempbuf = strdup_16to8(lpBufferW);
    mem_free(lpBufferW);
    if (UNLIKELY(!tempbuf)) {
        DLOG("Out of memory converting to UTF-8");
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
    retval = strlen(tempbuf);
    if (!strformat_check(lpBuffer, nBufferLength, "%s", tempbuf)) {
        retval++;  // Include the null terminator in the overflow return value.
    }
    mem_free(tempbuf);
    return retval;
}

/*-----------------------------------------------------------------------*/

extern char *GetEnvironmentStringsU(void)
{
    uint16_t *envp_16 = GetEnvironmentStringsW();
    if (UNLIKELY(!envp_16)) {
        return NULL;
    }

    const uint16_t *s16 = envp_16;
    char *envp = NULL;
    int envp_size = 0;
    while (*s16) {
        char *s = strdup_16to8(s16);
        if (UNLIKELY(!s)) {
            DLOG("Out of memory converting to UTF-8");
            mem_free(envp);
            FreeEnvironmentStringsW(envp_16);
            SetLastError(ERROR_OUTOFMEMORY);
            return NULL;
        }
        while (*s16++) { /*loop*/ }
        const int s_size = strlen(s) + 1;
        char *new_envp = mem_realloc(envp, envp_size + s_size, 0);
        if (UNLIKELY(!new_envp)) {
            DLOG("Failed to expand envp for variable %s (size=%d)",
                 s, envp_size + s_size);
            mem_free(s);
            mem_free(envp);
            FreeEnvironmentStringsW(envp_16);
            SetLastError(ERROR_OUTOFMEMORY);
            return NULL;
        }
        envp = new_envp;
        memcpy(envp + envp_size, s, s_size);
        envp_size += s_size;
        mem_free(s);
    }
    FreeEnvironmentStringsW(envp_16);

    char *new_envp = mem_realloc(envp, envp_size+1, 0);
    if (UNLIKELY(!new_envp)) {
        DLOG("Failed to expand envp for final null byte (size=%d)",
             envp_size+1);
        mem_free(envp);
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    envp = new_envp;
    envp[envp_size] = '\0';
    return envp;
}

/*-----------------------------------------------------------------------*/

extern DWORD GetEnvironmentVariableU(const char *lpName, char *lpBuffer,
                                     DWORD nSize)
{
    uint16_t *lpNameW = strdup_8to16(lpName);
    if (UNLIKELY(!lpNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpName);
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
    uint16_t *lpBufferW;
    if (nSize > 0) {
        ASSERT(lpBuffer != NULL,
               SetLastError(ERROR_INVALID_PARAMETER); return 0);
        lpBufferW = mem_alloc(nSize*2, 2, MEM_ALLOC_TEMP);
        if (UNLIKELY(!lpBufferW)) {
            DLOG("Out of memory allocating %d-element UTF-16 buffer",
                 (int)nSize);
            mem_free(lpNameW);
            SetLastError(ERROR_OUTOFMEMORY);
            return 0;
        }
    } else {
        lpBufferW = NULL;
    }
    DWORD retval = GetEnvironmentVariableW(lpNameW, lpBufferW, nSize);
    if (retval == 0) {
        mem_free(lpNameW);
        mem_free(lpBufferW);
        return 0;
    }
    if (retval > nSize) {
        /* We need to return the final UTF-8 size, so we need to retry the
         * get with a sufficiently large buffer. */
        mem_free(lpBufferW);
        lpBufferW = mem_alloc(retval*2, 2, MEM_ALLOC_TEMP);
        if (UNLIKELY(!lpBufferW)) {
            DLOG("Out of memory allocating %d-element UTF-16 buffer",
                 (int)retval);
            mem_free(lpNameW);
            SetLastError(ERROR_OUTOFMEMORY);
            return 0;
        }
        DWORD retval2 = GetEnvironmentVariableW(lpNameW, lpBufferW, retval);
        ASSERT(retval2 == retval - 1,
               SetLastError(ERROR_BAD_ENVIRONMENT); return 0);
    }
    char *tempbuf = strdup_16to8(lpBufferW);
    mem_free(lpNameW);
    mem_free(lpBufferW);
    if (UNLIKELY(!tempbuf)) {
        DLOG("Out of memory converting to UTF-8");
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
    retval = strlen(tempbuf);
    if (!strformat_check(lpBuffer, nSize, "%s", tempbuf)) {
        retval++;  // Include the null terminator in the overflow return value.
    }
    mem_free(tempbuf);
    return retval;
}

/*-----------------------------------------------------------------------*/

DWORD GetFileAttributesU(const char *lpFileName)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD retval = GetFileAttributesW(lpFileNameW);
    mem_free(lpFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL GetMessageU(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
                 UINT wMsgFilterMax)
{
    return GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}

/*-----------------------------------------------------------------------*/

DWORD GetModuleFileNameU(HMODULE hModule, char *lpFilename, DWORD nSize)
{
    if (nSize == 0) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    uint16_t *lpFilenameW = mem_alloc(nSize*2, 2, MEM_ALLOC_TEMP);
    if (UNLIKELY(!lpFilenameW)) {
        DLOG("Out of memory allocating %d-element UTF-16 buffer", nSize);
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
    DWORD length16 = GetModuleFileNameW(hModule, lpFilenameW, nSize);
    if (UNLIKELY(!length16)) {
        mem_free(lpFilenameW);
        return 0;
    }
    if (length16 >= nSize) {
        lpFilenameW[nSize-1] = 0;  // Windows XP doesn't terminate the string.
    }
    char *temp = strdup_16to8(lpFilenameW);
    mem_free(lpFilenameW);
    if (UNLIKELY(!temp)) {
        DLOG("Out of memory converting to UTF-8");
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }
    DWORD retval;
    if (strlen(temp) >= nSize) {
        retval = nSize;
        memcpy(lpFilename, temp, nSize-1);
        lpFilename[nSize-1] = '\0';
    } else {
        retval = strlen(temp);
        memcpy(lpFilename, temp, retval);
        lpFilename[retval] = '\0';
    }
    mem_free(temp);
    return retval;
}

/*-----------------------------------------------------------------------*/

UINT GetRawInputDeviceInfoU(HANDLE hDevice, UINT uiCommand,
                            LPVOID pData, PUINT pcbSize)
{
    if (uiCommand != RIDI_DEVICENAME) {
        /* No difference in behavior for this case. */
        return GetRawInputDeviceInfoW(hDevice, uiCommand, pData, pcbSize);
    }

    const UINT original_size = *pcbSize;
    uint16_t *name16;
    void *pDataW;
    if (pData != NULL && *pcbSize > 0) {
        /* This function fails if the buffer is not (DWORD? pointer?)
         * aligned -- presumably an alignment check for structures that's
         * incorrectly applied to strings as well. */
        name16 = mem_alloc((*pcbSize)*2, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!name16)) {
            DLOG("Out of memory allocating %u-element UTF-16 buffer",
                 *pcbSize);
            SetLastError(ERROR_OUTOFMEMORY);
            return (UINT)(-1);
        }
        pDataW = name16;
    } else {
        name16 = NULL;
        pDataW = pData;
    }
    UINT retval = GetRawInputDeviceInfoW(hDevice, uiCommand, pDataW, pcbSize);

    /* The MSDN documentation doesn't specify the error code returned if
     * the caller's buffer is too small, but we assume it behaves like
     * GetRawInputDeviceList() and returns ERROR_INSUFFICIENT_BUFFER. */
    if (retval == (UINT)(-1) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return retval;
    }

    /* In order to return the correct byte count for pData==NULL or
     * overflow cases, we need to obtain the actual string and convert it.
     * Even if we already had a buffer, we can't assume it's large enough
     * for the converted string, so we convert to a temporary buffer and
     * copy from there. */
    if (pData == NULL || retval == (UINT)(-1)) {
        mem_free(name16);
        name16 = mem_alloc((*pcbSize)*2, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!name16)) {
            DLOG("Out of memory allocating %u-element UTF-16 buffer",
                 *pcbSize);
            SetLastError(ERROR_OUTOFMEMORY);
            return (UINT)(-1);
        }
        retval = GetRawInputDeviceInfoW(hDevice, uiCommand, name16, pcbSize);
        if (retval == (UINT)(-1)) {
            mem_free(name16);
            return retval;
        }
        retval = (pData == NULL ? 0 : (UINT)(-1));
    }
    char *name = strdup_16to8(name16);
    mem_free(name16);
    if (UNLIKELY(!name)) {
        DLOG("Out of memory converting to UTF-8");
        SetLastError(ERROR_OUTOFMEMORY);
        return (UINT)(-1);
    }
    *pcbSize = strlen(name) + 1;
    if (pData != NULL) {
        if (original_size >= *pcbSize) {
            memcpy(pData, name, *pcbSize);
            retval = *pcbSize;
        } else {
            retval = (UINT)(-1);
        }
    }
    mem_free(name);

    return retval;
}

/*-----------------------------------------------------------------------*/

DWORD GetTempPathU(DWORD nBufferLength, char *lpBuffer)
{
    uint16_t buffer16[MAX_PATH+1];
    char buffer8[(MAX_PATH+1)*3];
    DWORD length16 = GetTempPathW(lenof(buffer16), buffer16);
    if (UNLIKELY(!length16)) {
        return 0;
    }
    ASSERT(length16 < lenof(buffer16), return 0);  // Guaranteed by API.
    convert_16to8(buffer16, buffer8);
    const int length = strlen(buffer8);
    if (length+1 <= (int)nBufferLength) {
        memcpy(lpBuffer, buffer8, length+1);
        return length;
    } else {
        /* The API documentation doesn't specify exactly what happens when
         * the buffer is too small, but testing with Windows XP indicates
         * that the buffer is left unmodified and the return value includes
         * the trailing null character. */
        return length+1;
    }
}

/*-----------------------------------------------------------------------*/

LONG GetWindowLongU(HWND hWnd, int nIndex)
{
    return GetWindowLongW(hWnd, nIndex);
}

/*-----------------------------------------------------------------------*/

HMODULE LoadLibraryU(const char *lpFileName)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    HMODULE retval = LoadLibraryW(lpFileNameW);
    mem_free(lpFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL MoveFileExU(const char *lpExistingFileName,
                 const char *lpNewFileName,
                 DWORD dwFlags)
{
    uint16_t *lpExistingFileNameW = strdup_8to16(lpExistingFileName);
    if (UNLIKELY(!lpExistingFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpExistingFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    uint16_t *lpNewFileNameW = strdup_8to16(lpNewFileName);
    if (UNLIKELY(!lpNewFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpNewFileName);
        mem_free(lpExistingFileNameW);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = MoveFileExW(lpExistingFileNameW, lpNewFileNameW, dwFlags);
    mem_free(lpNewFileNameW);
    mem_free(lpExistingFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL PeekMessageU(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
                 UINT wMsgFilterMax, UINT wRemoveMsg)
{
    return PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

/*-----------------------------------------------------------------------*/

BOOL PostMessageU(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return PostMessageW(hWnd, Msg, wParam, lParam);
}

/*-----------------------------------------------------------------------*/

ATOM RegisterClassExU(const WNDCLASSEXU *lpwcx)
{
    WNDCLASSEXW wcx16;
    STATIC_ASSERT(sizeof(wcx16) == sizeof(*lpwcx), "Bad sizeof(WNDCLASSEXU)");
    memcpy(&wcx16, lpwcx, sizeof(*lpwcx));

    uint16_t *menuName16 = NULL;
    if (lpwcx->lpszMenuName && (uintptr_t)lpwcx->lpszMenuName >= 0x10000) {
        menuName16 = strdup_8to16(lpwcx->lpszMenuName);
        wcx16.lpszMenuName = menuName16;
    }

    uint16_t *className16 = NULL;
    if (lpwcx->lpszClassName && (uintptr_t)lpwcx->lpszClassName >= 0x10000) {
        className16 = strdup_8to16(lpwcx->lpszClassName);
        wcx16.lpszClassName = className16;
    }

    ATOM result = RegisterClassExW(&wcx16);
    mem_free(className16);
    mem_free(menuName16);
    return result;
}

/*-----------------------------------------------------------------------*/

BOOL RemoveDirectoryU(const char *lpPathName)
{
    uint16_t *lpPathNameW = strdup_8to16(lpPathName);
    if (UNLIKELY(!lpPathNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpPathName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = RemoveDirectoryW(lpPathNameW);
    mem_free(lpPathNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

HRESULT SHGetFolderPathU(HWND hwndOwner, int nFolder, HANDLE hToken,
                         DWORD dwFlags, char *pszPath)
{
    uint16_t pszPathW[MAX_PATH];
    HRESULT retval = SHGetFolderPathW(hwndOwner, nFolder, hToken, dwFlags,
                                      pszPathW);
    convert_16to8(pszPathW, pszPath);
    return retval;
}

/*-----------------------------------------------------------------------*/

LRESULT SendMessageU(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return SendMessageW(hWnd, Msg, wParam, lParam);
}

/*-----------------------------------------------------------------------*/

BOOL SetCurrentDirectoryU(const char *lpPathName)
{
    uint16_t *lpPathNameW = strdup_8to16(lpPathName);
    if (UNLIKELY(!lpPathNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpPathName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = SetCurrentDirectoryW(lpPathNameW);
    mem_free(lpPathNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

extern BOOL SetEnvironmentVariableU(const char *lpName, const char *lpValue)
{
    uint16_t *lpNameW = strdup_8to16(lpName);
    if (UNLIKELY(!lpNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    uint16_t *lpValueW;
    if (lpValue) {
        lpValueW = strdup_8to16(lpValue);
        if (UNLIKELY(!lpValueW)) {
            DLOG("Out of memory converting to UTF-16: %s", lpValue);
            mem_free(lpNameW);
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    } else {
        lpValueW = NULL;
    }
    BOOL retval = SetEnvironmentVariableW(lpNameW, lpValueW);
    mem_free(lpValueW);
    mem_free(lpNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL SetFileAttributesU(const char *lpFileName, DWORD dwFileAttributes)
{
    uint16_t *lpFileNameW = strdup_8to16(lpFileName);
    if (UNLIKELY(!lpFileNameW)) {
        DLOG("Out of memory converting to UTF-16: %s", lpFileName);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    BOOL retval = SetFileAttributesW(lpFileNameW, dwFileAttributes);
    mem_free(lpFileNameW);
    return retval;
}

/*-----------------------------------------------------------------------*/

BOOL SetWindowLongU(HWND hWnd, int nIndex, LONG dwNewLong)
{
    return SetWindowLongW(hWnd, nIndex, dwNewLong);
}

/*-----------------------------------------------------------------------*/

BOOL SetWindowTextU(HWND hWnd, const char *lpString)
{
    uint16_t *lpStringW;
    if (lpString) {
        lpStringW = strdup_8to16(lpString);
        if (UNLIKELY(!lpStringW)) {
            DLOG("Out of memory converting to UTF-16: %s", lpString);
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
    } else {
        lpStringW = NULL;
    }
    BOOL retval = SetWindowTextW(hWnd, lpStringW);
    mem_free(lpStringW);
    return retval;
}

/*************************************************************************/
/*************************************************************************/
