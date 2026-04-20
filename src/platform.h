typedef struct HTTP_Response HTTP_Response;
struct HTTP_Response
{
    i64 status;
    String body;
    String headers;
};

typedef struct HTTP_Response_Array HTTP_Response_Array;
struct HTTP_Response_Array
{
    _ArrayHeader_;
    HTTP_Response *data;
};

typedef struct MenuItem MenuItem;

typedef struct MenuItem_Array MenuItem_Array;
struct MenuItem_Array
{
    _ArrayHeader_;
    MenuItem *data;
};

struct MenuItem
{
    u32 id;

    b32 disabled;
    b32 checked;
    b32 radio;
    b32 hidden;
    b32 separator;

    String name;
    String shortcut;

    MenuItem_Array subitems;
};

typedef struct Platform_Handle Platform_Handle;
struct Platform_Handle
{
    void *handle;
};

function HTTP_Response platform__http_get(Arena *arena, String url, String headers);
function HTTP_Response platform__http_post(Arena *arena, String url, String body, String headers);

function Platform_Handle platform__http_server_open(Arena *arena, u16 *port);
function String platform__http_server_pump(Arena *arena, Platform_Handle handle);
function void platform__http_server_close(Platform_Handle handle);

function String platform__get_google_token(Arena *arena, String client_id, String client_secret);
function String platform__refresh_google_token(Arena *arena, String client_id, String client_secret, String token);

function i64 platform__show_menu(MenuItem *items, u64 count, i32 x, i32 y);

function void platform__quit();

MenuItem *menu_push(Arena *arena, MenuItem *parent, MenuItem item)
{
    array_push(arena, &parent->subitems, item);
    return array_peek(&parent->subitems);
}