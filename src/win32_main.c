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

typedef struct Platform_HTTP_Response Platform_HTTP_Response;
struct Platform_HTTP_Response
{
    i64 status;
    String body;
    String headers;
};

function Platform_HTTP_Response platform__http_get(Arena *arena, String url, String headers)
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

    Platform_HTTP_Response resp = {0};
    resp.status = status;
    resp.body = result;
    resp.headers = resp_headers;
    return resp;
}

function Platform_HTTP_Response platform__http_post(Arena *arena, String url, String body, String headers)
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

    Platform_HTTP_Response resp = {0};
    resp.status = status;
    resp.body = result;
    resp.headers = resp_headers;
    return resp;
}

typedef struct Win32_Platform_HTTP Win32_Platform_HTTP;
struct Win32_Platform_HTTP
{
    SOCKET server;
    SOCKET client;
    b32    is_open;
};

typedef struct Platform_Handle Platform_Handle;
struct Platform_Handle
{
    void *handle;
};

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

    Platform_HTTP_Response response = platform__http_post(arena, S("https://oauth2.googleapis.com/token"), body, S("Content-Type: application/x-www-form-urlencoded"));
    return response.body;
}

function String platform__refresh_google_token(Arena *arena, String client_id, String client_secret, String token)
{
    JSON_Element *root = json_parse(arena, token);
    if (!root) return S("");
    String refresh_token = json_to_string(json_find(root, S("refresh_token")));

    String body = string_print(arena,
        "grant_type=refresh_token"
        "&refresh_token=%.*s"
        "&client_id=%.*s"
        "&client_secret=%.*s",
        LIT(refresh_token), LIT(client_id), LIT(client_secret));

    Platform_HTTP_Response resp = platform__http_post(arena, S("https://oauth2.googleapis.com/token"), body, S("Content-Type: application/x-www-form-urlencoded"));
    return resp.body;
}

#include "app.c"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    os_init();
    app_run();

    WSACleanup();
    return 0;
}