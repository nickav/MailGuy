#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb/stb_sprintf.h"

#define impl
#include "third_party/na/na.h"
#include "third_party/na/json.h"
#include "third_party/base64.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <curl/curl.h>

#include "platform.h"

typedef struct MacOS_Platform_HTTP MacOS_Platform_HTTP;
struct MacOS_Platform_HTTP
{
    int server;
    int client;
    b32 is_open;
};

typedef struct Curl__Buf Curl__Buf;
struct Curl__Buf
{
    Arena *arena;
    String data;
};

static size_t curl__write_fn(char *ptr, size_t size, size_t nmemb, void *ud)
{
    Curl__Buf *buf = (Curl__Buf *)ud;
    size_t n = size * nmemb;
    u8 *dest = PushArrayNoZero(buf->arena, u8, n);
    memcpy(dest, ptr, n);
    if (!buf->data.data) buf->data.data = dest;
    buf->data.count += n;
    return n;
}

static struct curl_slist *curl__headers_from_string(M_Temp *scratch, String headers)
{
    struct curl_slist *list = NULL;
    String rest = headers;
    while (rest.count > 0)
    {
        i64 end = string_index(rest, S("\r\n"), 0);
        String line = (end >= 0) ? string_prefix(rest, end) : rest;
        if (line.count)
        {
            char *cstr = (char *)PushArrayNoZero(scratch->arena, u8, line.count + 1);
            memcpy(cstr, line.data, line.count);
            cstr[line.count] = 0;
            list = curl_slist_append(list, cstr);
        }
        if (end < 0) break;
        rest = string_skip(rest, end + 2);
    }
    return list;
}

function HTTP_Response platform__http_get(Arena *arena, String url, String headers)
{
    M_Temp scratch = GetScratch(&arena, 1);

    char *url_cstr = (char *)PushArrayNoZero(scratch.arena, u8, url.count + 1);
    memcpy(url_cstr, url.data, url.count);
    url_cstr[url.count] = 0;

    Curl__Buf body_buf = {arena, {0}};
    Curl__Buf hdrs_buf = {arena, {0}};

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,            url_cstr);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl__write_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl__write_fn);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA,     &hdrs_buf);

    struct curl_slist *header_list = NULL;
    if (headers.count)
    {
        header_list = curl__headers_from_string(&scratch, headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    ReleaseScratch(scratch);

    HTTP_Response resp = {0};
    resp.status  = status;
    resp.body    = body_buf.data;
    resp.headers = hdrs_buf.data;
    return resp;
}

function HTTP_Response platform__http_post(Arena *arena, String url, String body, String headers)
{
    M_Temp scratch = GetScratch(&arena, 1);

    char *url_cstr = (char *)PushArrayNoZero(scratch.arena, u8, url.count + 1);
    memcpy(url_cstr, url.data, url.count);
    url_cstr[url.count] = 0;

    Curl__Buf body_buf = {arena, {0}};
    Curl__Buf hdrs_buf = {arena, {0}};

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,            url_cstr);
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)body.count);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl__write_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &body_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl__write_fn);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA,     &hdrs_buf);

    struct curl_slist *header_list = NULL;
    if (headers.count)
    {
        header_list = curl__headers_from_string(&scratch, headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    ReleaseScratch(scratch);

    HTTP_Response resp = {0};
    resp.status  = status;
    resp.body    = body_buf.data;
    resp.headers = hdrs_buf.data;
    return resp;
}

function Platform_Handle platform__http_server_open(Arena *arena, u16 *port)
{
    MacOS_Platform_HTTP *http = PushStruct(arena, MacOS_Platform_HTTP);
    http->server = socket(AF_INET, SOCK_STREAM, 0);
    http->client = -1;

    int reuse = 1;
    setsockopt(http->server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(*port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    bind(http->server, (struct sockaddr *)&addr, sizeof(addr));
    socklen_t addr_len = sizeof(addr);
    getsockname(http->server, (struct sockaddr *)&addr, &addr_len);
    if (port) *port = ntohs(addr.sin_port);

    listen(http->server, 1);
    http->is_open = true;

    fcntl(http->server, F_SETFL, O_NONBLOCK);

    Platform_Handle result = {http};
    return result;
}

function String platform__http_server_pump(Arena *arena, Platform_Handle handle)
{
    MacOS_Platform_HTTP *http = (MacOS_Platform_HTTP *)handle.handle;
    if (!http || !http->is_open) return S("");

    http->client = accept(http->server, NULL, NULL);
    if (http->client < 0) return S("");

    String result  = {0};
    u8 *buf        = PushArray(arena, u8, 4096);
    int bytes_read = (int)recv(http->client, (char *)buf, 4095, 0);
    if (bytes_read > 0)
    {
        result = string_make(buf, bytes_read);
    }

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

    close(http->client);
    http->client = -1;

    return result;
}

function void platform__http_server_close(Platform_Handle handle)
{
    MacOS_Platform_HTTP *http = (MacOS_Platform_HTTP *)handle.handle;
    if (!http) return;

    if (http->client >= 0) close(http->client);
    if (http->server >= 0) close(http->server);
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

#include "app.c"

static bool          g_is_running  = true;
static long          g_menu_result = -1;
static NSStatusItem *g_status_item = nil;

@interface MenuTarget : NSObject
- (void)menuAction:(NSMenuItem *)sender;
- (void)trayClicked:(id)sender;
@end

@implementation MenuTarget

- (void)menuAction:(NSMenuItem *)sender
{
    g_menu_result = sender.tag;
}

- (void)trayClicked:(id)sender
{
    NSPoint loc    = [NSEvent mouseLocation];
    CGRect  bounds = CGDisplayBounds(CGMainDisplayID());
    app_menu((i32)loc.x, (i32)(bounds.size.height - loc.y));
}

@end

static MenuTarget *g_menu_target = nil;

static NSMenu *macos__menu_build(Arena *arena, MenuItem *items, u64 count)
{
    NSMenu *menu = [[NSMenu alloc] init];
    menu.autoenablesItems = NO;

    for (u64 i = 0; i < count; i++)
    {
        MenuItem *it = &items[i];
        if (it->hidden) continue;

        if (it->separator)
        {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }

        char *cstr = (char *)PushArrayNoZero(arena, u8, it->name.count + 1);
        memcpy(cstr, it->name.data, it->name.count);
        cstr[it->name.count] = 0;

        NSMenuItem *item = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:cstr]
                   action:@selector(menuAction:)
            keyEquivalent:@""];

        item.tag    = (NSInteger)it->id;
        item.target = g_menu_target;

        if (it->disabled) item.enabled = NO;
        if (it->checked)  item.state   = NSControlStateValueOn;

        if (it->subitems.data && it->subitems.count > 0)
        {
            item.submenu = macos__menu_build(arena, it->subitems.data, it->subitems.count);
        }

        [menu addItem:item];
    }

    return menu;
}

function i64 platform__show_menu(MenuItem *items, u64 count, i32 x, i32 y)
{
    M_Temp scratch = GetScratch(0, 0);

    NSMenu *menu  = macos__menu_build(scratch.arena, items, count);
    CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    NSPoint pt    = NSMakePoint((CGFloat)x, bounds.size.height - (CGFloat)y);

    g_menu_result = -1;
    [menu popUpMenuPositioningItem:nil atLocation:pt inView:nil];

    i64 result = g_menu_result;
    ReleaseScratch(scratch);
    return result;
}

static void macos__tray_init(void)
{
    g_menu_target = [[MenuTarget alloc] init];

    g_status_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    [g_status_item retain];

    NSImage *image = nil;
    if (@available(macOS 11.0, *))
    {
        image = [NSImage imageWithSystemSymbolName:@"envelope" accessibilityDescription:nil];
    }
    if (!image)
    {
        image = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];
        [image lockFocus];
        [@"✉" drawAtPoint:NSZeroPoint withAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:14]}];
        [image unlockFocus];
    }
    image.template = YES;

    NSStatusBarButton *button = g_status_item.button;
    button.image  = image;
    button.target = g_menu_target;
    button.action = @selector(trayClicked:);
    [button sendActionOn:NSEventMaskLeftMouseDown|NSEventMaskRightMouseDown];
}

function void platform__quit()
{
    if (g_status_item)
    {
        [[NSStatusBar systemStatusBar] removeStatusItem:g_status_item];
        g_status_item = nil;
    }
    g_is_running = false;
}

int main(int argc, char **argv)
{
    curl_global_init(CURL_GLOBAL_ALL);
    os_init();

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];

    macos__tray_init();

    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    // NOTE(nick): run in another thread so the tray icon can be interactive...
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        app_init();
    });

    NSDate *past = [NSDate distantPast];
    NSString *mode = NSDefaultRunLoopMode;
    while (g_is_running)
    {
        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:past inMode:mode dequeue:YES]))
        {
            [NSApp sendEvent:event];
        }
        app_tick();
        usleep(16000);
    }

    app_quit();
    curl_global_cleanup();
    return 0;
}