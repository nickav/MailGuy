#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb/stb_sprintf.h"

#define impl
#include "third_party/na/na.h"
#include "third_party/na/json.h"
#include "third_party/base64.h"

#include <winhttp.h>
#include <winsock2.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

#include <shellapi.h>

static NOTIFYICONDATAW g_nid = {0};
static bool g_is_running = true;
static HWND g_hwnd = NULL;

#define IDI_MYICON 101
#define WM_TRAY (WM_USER + 1)

#include "platform.h"

typedef struct Win32_Platform_HTTP Win32_Platform_HTTP;
struct Win32_Platform_HTTP
{
    SOCKET server;
    SOCKET client;
    b32    is_open;
};

function HTTP_Response platform__http_get(Arena *arena, String url, String headers)
{
    bool is_https = string_starts_with(url, S("https://"));
    DWORD port = is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;

    if (string_starts_with(url, S("https://"))) url = string_skip(url, S("https://").count);
    if (string_starts_with(url, S("http://"))) url = string_skip(url, S("http://").count);

    i64 slash = string_index(url, S("/"), 0);
    String host = slash >= 0 ? string_prefix(url, slash) : url;
    String path = slash >= 0 ? string_skip(url, slash)   : S("/");
    i64 colon = string_index(host, S(":"), 0);
    if (colon >= 0)
    {
        String port_str = string_skip(host, colon + 1);
        port = (DWORD)string_to_i64(port_str, 10);
        host = string_prefix(host, colon);
    }

    M_Temp scratch = GetScratch(&arena, 1);

    String16 host16 = string16_from_string(scratch.arena, host);
    String16 path16 = string16_from_string(scratch.arena, path);

    HINTERNET session = WinHttpOpen(L"email_guy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = WinHttpConnect(session, (WCHAR *)host16.data, port, 0);
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", (WCHAR *)path16.data, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (headers.count) {
        String16 headers16 = string16_from_string(scratch.arena, headers);
        WinHttpAddRequestHeaders(request, (WCHAR *)headers16.data, -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL sent     = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    BOOL received = WinHttpReceiveResponse(request, NULL);
    // print("sent=%d received=%d error=%d\n", sent, received, GetLastError());

    DWORD status = 0;
    DWORD size   = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);

    DWORD headers_size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &headers_size, WINHTTP_NO_HEADER_INDEX);

    u16 *headers_buf = PushArrayNoZero(arena, u16, headers_size / sizeof(u16));
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, headers_buf, &headers_size, WINHTTP_NO_HEADER_INDEX);

    String16 resp_headers16 = Str16(headers_buf, headers_size / sizeof(u16));
    String resp_headers = string_from_string16(arena, resp_headers16);

    String result = {0};
    DWORD bytes = 0;
    do {
        WinHttpQueryDataAvailable(request, &bytes);
        if (!bytes) break;
        u8 *dest = PushArrayNoZero(arena, u8, bytes);
        WinHttpReadData(request, dest, bytes, &bytes);
        if (!result.data) result.data = dest;
        result.count += bytes;
    } while (bytes > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    ReleaseScratch(scratch);

    HTTP_Response resp = {0};
    resp.status = status;
    resp.body = result;
    resp.headers = resp_headers;
    return resp;
}

function HTTP_Response platform__http_post(Arena *arena, String url, String body, String headers)
{
    bool is_https = string_starts_with(url, S("https://"));
    DWORD port = is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;

    if (string_starts_with(url, S("https://"))) url = string_skip(url, S("https://").count);
    if (string_starts_with(url, S("http://"))) url = string_skip(url, S("http://").count);

    i64 slash = string_index(url, S("/"), 0);
    String host = slash >= 0 ? string_prefix(url, slash) : url;
    String path = slash >= 0 ? string_skip(url, slash)   : S("/");
    i64 colon = string_index(host, S(":"), 0);
    if (colon >= 0)
    {
        String port_str = string_skip(host, colon + 1);
        port = (DWORD)string_to_i64(port_str, 10);
        host = string_prefix(host, colon);
    }

    M_Temp scratch = GetScratch(&arena, 1);

    String16 host16 = string16_from_string(scratch.arena, host);
    String16 path16 = string16_from_string(scratch.arena, path);

    HINTERNET session = WinHttpOpen(L"email_guy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = WinHttpConnect(session, (WCHAR *)host16.data, port, 0);
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", (WCHAR *)path16.data, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (headers.count) {
        String16 headers16 = string16_from_string(scratch.arena, headers);
        WinHttpAddRequestHeaders(request, (WCHAR *)headers16.data, -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.data, (DWORD)body.count, (DWORD)body.count, 0);
    WinHttpReceiveResponse(request, NULL);

    DWORD status = 0;
    DWORD size   = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);

    DWORD headers_size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &headers_size, WINHTTP_NO_HEADER_INDEX);

    u16 *headers_buf = PushArrayNoZero(arena, u16, headers_size / sizeof(u16));
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, headers_buf, &headers_size, WINHTTP_NO_HEADER_INDEX);

    String16 resp_headers16 = Str16(headers_buf, headers_size / sizeof(u16));
    String resp_headers = string_from_string16(arena, resp_headers16);

    String result = {0};
    DWORD bytes = 0;
    do {
        WinHttpQueryDataAvailable(request, &bytes);
        if (!bytes) break;
        u8 *dest = PushArrayNoZero(arena, u8, bytes);
        WinHttpReadData(request, dest, bytes, &bytes);
        if (!result.data) result.data = dest;
        result.count += bytes;
    } while (bytes > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    ReleaseScratch(scratch);

    HTTP_Response resp = {0};
    resp.status = status;
    resp.body = result;
    resp.headers = resp_headers;
    return resp;
}

function Platform_Handle platform__http_server_open(Arena *arena, u16 *port)
{
    Win32_Platform_HTTP *http = PushStruct(arena, Win32_Platform_HTTP);
    http->server = socket(AF_INET, SOCK_STREAM, 0);
    http->client = INVALID_SOCKET;

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(*port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    bind(http->server, (struct sockaddr *)&addr, sizeof(addr));
    int addr_len = sizeof(addr);
    getsockname(http->server, (struct sockaddr *)&addr, &addr_len);
    if (port) *port = ntohs(addr.sin_port);

    listen(http->server, 1);
    http->is_open = true;

    u_long mode = 1;
    ioctlsocket(http->server, FIONBIO, &mode);

    Platform_Handle result = {http};
    return result;
}

function String platform__http_server_pump(Arena *arena, Platform_Handle handle)
{
    Win32_Platform_HTTP *http = (Win32_Platform_HTTP *)handle.handle;
    if (!http || !http->is_open) return S("");

    http->client = accept(http->server, NULL, NULL);
    if (http->client == INVALID_SOCKET) return S("");

    String result  = {0};
    u8 *buf        = PushArray(arena, u8, 4096);
    int bytes_read = recv(http->client, (char *)buf, 4095, 0);
    if (bytes_read > 0)
    {
        result = string_make(buf, bytes_read);
    }

    // @Robustness: parameterize this?
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html><body style='font-family:sans-serif;text-align:center;padding:40px'>"
        "<h2>Success!</h2>"
        "<p>You can close this tab and return to the app.</p>"
        "</body></html>";
    send(http->client, response, (int)strlen(response), 0);

    closesocket(http->client);
    http->client = INVALID_SOCKET;

    return result;
}

function void platform__http_server_close(Platform_Handle handle)
{
    Win32_Platform_HTTP *http = (Win32_Platform_HTTP *)handle.handle;
    if (!http) return;

    if (http->client != INVALID_SOCKET) closesocket(http->client);
    if (http->server != INVALID_SOCKET) closesocket(http->server);
    http->is_open = false;
}

function String platform__get_google_token(Arena *arena, String client_id, String client_secret)
{
    u16 port = 0;
    Platform_Handle http = platform__http_server_open(arena, &port);

    String url = string_print(arena,
        "https://accounts.google.com/o/oauth2/v2/auth"
        "?client_id=%.*s"
        "&redirect_uri=http://localhost:%d"
        "&response_type=code"
        "&scope=https://www.googleapis.com/auth/gmail.readonly"
        "&access_type=offline"
        "&prompt=consent",
        LIT(client_id), port);

    os_shell_open(url);

    // print("Waiting for auth...\n");

    String request = S("");
    while (!request.count)
    {
        request = platform__http_server_pump(arena, http);
        os_sleep(0.05);
    }

    platform__http_server_close(http);

    i64 code_start = string_index(request, S("code="), 0) + S("code=").count;
    i64 code_end   = string_find(request, S("&"), code_start, 0);
    if (code_end >= request.count) code_end = string_index(request, S(" "), code_start);
    String code = string_slice(request, code_start, code_end);

    // print("Got code: %.*s\n", LIT(code));

    String body = string_print(arena,
        "grant_type=authorization_code"
        "&code=%.*s"
        "&client_id=%.*s"
        "&client_secret=%.*s"
        "&redirect_uri=http://localhost:%d",
        LIT(code), LIT(client_id), LIT(client_secret), port);

    HTTP_Response response = platform__http_post(arena, S("https://oauth2.googleapis.com/token"), body, S("Content-Type: application/x-www-form-urlencoded"));
    return response.body;
}

function String platform__refresh_google_token(Arena *arena, String client_id, String client_secret, String refresh_token)
{
    String body = string_print(arena,
        "grant_type=refresh_token"
        "&refresh_token=%.*s"
        "&client_id=%.*s"
        "&client_secret=%.*s",
        LIT(refresh_token), LIT(client_id), LIT(client_secret));

    HTTP_Response resp = platform__http_post(arena, S("https://oauth2.googleapis.com/token"), body, S("Content-Type: application/x-www-form-urlencoded"));
    return resp.body;
}

static HMENU win32__menu_build(Arena *arena, MenuItem *items, u64 count)
{
    HMENU hmenu = CreatePopupMenu();

    for (u64 i = 0; i < count; i++)
    {
        MenuItem *it = &items[i];

        if (it->hidden) continue;

        if (it->separator)
        {
            AppendMenuW(hmenu, MF_SEPARATOR, 0, NULL);
            continue;
        }

        String label = it->name;
        if (it->shortcut.count > 0)
        {
            label = sprint("%.*s\t%.*s", LIT(it->name), LIT(it->shortcut));
        }

        String16 label16 = string16_from_string(arena, label);
        if (it->subitems.data && it->subitems.count > 0)
        {
            HMENU submenu = win32__menu_build(arena, it->subitems.data, it->subitems.count);

            UINT flags = MF_POPUP | MF_STRING;
            if (it->disabled) flags |= MF_GRAYED;
            AppendMenuW(hmenu, flags, (UINT_PTR)submenu, (LPCWSTR)label16.data);
        }
        else
        {
            MENUITEMINFOW info = {0};
            info.cbSize    = sizeof(info);
            info.fMask     = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
            info.fType     = it->radio ? (MFT_RADIOCHECK | MFT_STRING) : MFT_STRING;
            info.fState    = MFS_ENABLED;
            info.wID       = (UINT)(it->id + 1);
            info.dwTypeData = (LPWSTR)label16.data;

            if (it->checked)  info.fState |= MFS_CHECKED;
            if (it->disabled) info.fState |= MFS_DISABLED;

            InsertMenuItemW(hmenu, GetMenuItemCount(hmenu), TRUE, &info);
        }
    }

    return hmenu;
}

function i64 platform__show_menu(MenuItem *items, u64 count, i32 x, i32 y)
{
    M_Temp scratch = GetScratch(0, 0);
    HMENU hmenu = win32__menu_build(scratch.arena, items, count);

    UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY;
    SetForegroundWindow(g_hwnd);
    i64 result = (u32)TrackPopupMenuEx(hmenu, flags, x, y, g_hwnd, NULL);
    result -= 1;

    DestroyMenu(hmenu);
    ReleaseScratch(scratch);
    return result;
}

function void platform__quit()
{
    if (g_is_running)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_is_running = false;
    }
}

#include "app.c"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_TRAY:
        {
            if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP)
            {
                POINT pt;
                GetCursorPos(&pt);
                app_menu(pt.x, pt.y);
            }

            return 0;
        } break;

        case WM_DESTROY:
        {
            platform__quit();
            return 0;
        } break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    os_init();

    // NOTE(nick): Set DPI Awareness
    {
        HMODULE user32 = LoadLibraryA("user32.dll");

        typedef BOOL Win32_SetProcessDpiAwarenessContext(HANDLE);
        typedef BOOL Win32_SetProcessDpiAwareness(int);

        Win32_SetProcessDpiAwarenessContext *SetProcessDpiAwarenessContext = (Win32_SetProcessDpiAwarenessContext *) GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        Win32_SetProcessDpiAwareness *SetProcessDpiAwareness = (Win32_SetProcessDpiAwareness *) GetProcAddress(user32, "SetProcessDpiAwareness");

        if (SetProcessDpiAwarenessContext) {
            SetProcessDpiAwarenessContext(((HANDLE) -4) /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */);
        } else if (SetProcessDpiAwareness) {
            SetProcessDpiAwareness(1 /* PROCESS_SYSTEM_DPI_AWARE */);
        } else {
            SetProcessDPIAware();
        }
    }

    // NOTE(nick): Set Dark Mode Awareness
    {
        typedef DWORD WINAPI Win32_SetPreferredAppMode(DWORD);
        HMODULE uxtheme = LoadLibraryExA("uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (uxtheme)
        {
            Win32_SetPreferredAppMode *SetPreferredAppMode = (Win32_SetPreferredAppMode *)GetProcAddress(uxtheme, MAKEINTRESOURCEA(135));
            if (SetPreferredAppMode)
            {
                SetPreferredAppMode(1);
            }
        }
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"MailGuyTray";
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, L"MailGuyTray", L"MailGuyTray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, NULL);

    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon            = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    // g_nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MYICON));
    lstrcpyW(g_nid.szTip, L"Mail Guy");
    Shell_NotifyIconW(NIM_ADD, &g_nid);


    app_init();

    f64 then = os_time();
    while (g_is_running)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        f64 now = os_time();
        f64 dt = now - then;
        then = now;

        app_tick(dt);

        Sleep(16);
    }

    app_quit();

    WSACleanup();
    return 0;
}