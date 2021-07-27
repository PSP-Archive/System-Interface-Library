/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/utf8-wrappers.h: Declarations for UTF-8 wrappers on
 * Windows functions.
 */

#ifndef SIL_SRC_SYSDEP_WINDOWS_UTF8_WRAPPERS_H
#define SIL_SRC_SYSDEP_WINDOWS_UTF8_WRAPPERS_H

/*
 * These functions each map to the Unicode version of a single Windows
 * library or system call, but they take UTF-8 strings rather than the
 * UTF-16 strings required by ...W() functions, so callers do not need to
 * convert string parameters before every call.  Structures are similarly
 * defined so that string fields contain UTF-8 strings.
 */

/*************************************************************************/
/*************************************************************************/

typedef struct _WIN32_FIND_DATAU {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    /* We need the WCHAR versions here so we don't overwrite the part of
     * the structure that corresponds to WIN32_FIND_DATAW when converting
     * to UTF-8. */
    WCHAR cFileNameW[MAX_PATH];
    WCHAR cAlternateFileNameW[14];
    char cFileName[MAX_PATH*3];
    char cAlternateFileName[14*3];
} WIN32_FIND_DATAU, *PWIN32_FIND_DATAU, *LPWIN32_FIND_DATAU;
#undef WIN32_FIND_DATA
#define WIN32_FIND_DATA WIN32_FIND_DATAU
#undef PWIN32_FIND_DATA
#define PWIN32_FIND_DATA PWIN32_FIND_DATAU
#undef LPWIN32_FIND_DATA
#define LPWIN32_FIND_DATA LPWIN32_FIND_DATAU

typedef struct _WNDCLASSEXU {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    const char *lpszMenuName;
    const char *lpszClassName;
    HICON hIconSm;
} WNDCLASSEXU, *PWNDCLASSEXU, *LPWNDCLASSEXU;
#undef WNDCLASSEX
#define WNDCLASSEX WNDCLASSEXU
#undef PWNDCLASSEX
#define PWNDCLASSEX PWNDCLASSEXU
#undef LPWNDCLASSEX
#define LPWNDCLASSEX LPWNDCLASSEXU

/*-----------------------------------------------------------------------*/

extern BOOL CreateDirectoryU(const char *lpPathName,
                             LPSECURITY_ATTRIBUTES lpSecurityAttributes);
#undef CreateDirectory
#define CreateDirectory CreateDirectoryU

extern HANDLE CreateFileU(const char *lpFileName,
                          DWORD dwDesiredAccess,
                          DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                          DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile);
#undef CreateFile
#define CreateFile CreateFileU

extern HWND CreateWindowU(const char *lpClassName, const char *lpWindowName,
                          DWORD dwStyle, int x, int y, int nWidth, int nHeight,
                          HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                          LPVOID lpParam);
#undef CreateWindow
#define CreateWindow CreateWindowU

extern LRESULT DefWindowProcU(HWND hWnd, UINT Msg, WPARAM wParam,
                              LPARAM lParam);
#undef DefWindowProc
#define DefWindowProc DefWindowProcU

extern BOOL DeleteFileU(const char *lpFileName);
#undef DeleteFile
#define DeleteFile DeleteFileU

extern LRESULT DispatchMessageU(const MSG *lpMsg);
#undef DispatchMessage
#define DispatchMessage DispatchMessageU

extern HANDLE FindFirstFileU(const char *lpFileName,
                             LPWIN32_FIND_DATAU lpFindFileData);
#undef FindFirstFile
#define FindFirstFile FindFirstFileU

extern BOOL FindNextFileU(HANDLE hFindFile,
                          LPWIN32_FIND_DATAU lpFindFileData);
#undef FindNextFile
#define FindNextFile FindNextFileU

extern BOOL FreeEnvironmentStringsU(char *lpszEnvironmentBlock);
#undef FreeEnvironmentStrings
#define FreeEnvironmentStrings FreeEnvironmentStringsU

extern DWORD GetCurrentDirectoryU(DWORD nBufferLength, char *lpBuffer);
#undef GetCurrentDirectory
#define GetCurrentDirectory GetCurrentDirectoryU

extern char *GetEnvironmentStringsU(void);
#undef GetEnvironmentStrings
#define GetEnvironmentStrings GetEnvironmentStringsU

extern DWORD GetEnvironmentVariableU(const char *lpName, char *lpBuffer,
                                     DWORD nSize);
#undef GetEnvironmentVariable
#define GetEnvironmentVariable GetEnvironmentVariableU

extern DWORD GetFileAttributesU(const char *lpFileName);
#undef GetFileAttributes
#define GetFileAttributes GetFileAttributesU

extern BOOL GetMessageU(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
                        UINT wMsgFilterMax);
#undef GetMessage
#define GetMessage GetMessageU

extern UINT GetRawInputDeviceInfoU(HANDLE hDevice, UINT uiCommand,
                                   LPVOID pData, PUINT pcbSize);
#undef GetRawInputDeviceInfo
#define GetRawInputDeviceInfo GetRawInputDeviceInfoU

extern DWORD GetModuleFileNameU(HMODULE hModule, char *lpFilename,
                                DWORD nSize);
#undef GetModuleFileName
#define GetModuleFileName GetModuleFileNameU

extern DWORD GetTempPathU(DWORD nBufferLength, char *lpBuffer);
#undef GetTempPath
#define GetTempPath GetTempPathU

extern LONG GetWindowLongU(HWND hWnd, int nIndex);
#undef GetWindowLong
#define GetWindowLong GetWindowLongU

extern HMODULE LoadLibraryU(const char *lpFileName);
#undef LoadLibrary
#define LoadLibrary LoadLibraryU

extern BOOL MoveFileExU(const char *lpExistingFileName,
                        const char *lpNewFileName,
                        DWORD dwFlags);
#undef MoveFileEx
#define MoveFileEx MoveFileExU

extern BOOL PeekMessageU(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
                        UINT wMsgFilterMax, UINT wRemoveMsg);
#undef PeekMessage
#define PeekMessage PeekMessageU

extern BOOL PostMessageU(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#undef PostMessage
#define PostMessage PostMessageU

extern ATOM RegisterClassExU(const WNDCLASSEXU *lpwcx);
#undef RegisterClassEx
#define RegisterClassEx RegisterClassExU

extern BOOL RemoveDirectoryU(const char *lpFileName);
#undef RemoveDirectory
#define RemoveDirectory RemoveDirectoryU

/* The pszPath buffer must be at least MAX_PATH*3+1 bytes long. */
extern HRESULT SHGetFolderPathU(HWND hwndOwner, int nFolder, HANDLE hToken,
                                DWORD dwFlags, char *pszPath);
#undef SHGetFolderPath
#define SHGetFolderPath SHGetFolderPathU

extern LRESULT SendMessageU(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#undef SendMessage
#define SendMessage SendMessageU

extern BOOL SetCurrentDirectoryU(const char *lpPathName);
#undef SetCurrentDirectory
#define SetCurrentDirectory SetCurrentDirectoryU

extern BOOL SetEnvironmentVariableU(const char *lpName, const char *lpValue);
#undef SetEnvironmentVariable
#define SetEnvironmentVariable SetEnvironmentVariableU

extern BOOL SetFileAttributesU(const char *lpFileName, DWORD dwFileAttributes);
#undef SetFileAttributes
#define SetFileAttributes SetFileAttributesU

extern BOOL SetWindowLongU(HWND hWnd, int nIndex, LONG dwNewLong);
#undef SetWindowLong
#define SetWindowLong SetWindowLongU

extern BOOL SetWindowTextU(HWND hWnd, const char *lpString);
#undef SetWindowText
#define SetWindowText SetWindowTextU

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_WINDOWS_UTF8_WRAPPERS_H
