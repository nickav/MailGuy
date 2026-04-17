void D_(String label, String x)
{
    print("%.*s = %.*s\n", LIT(label), LIT(x));
}

#define D(x) D_(string_from_cstr(#x), (x))

Platform_HTTP_Response auth_get(Arena *arena, String url, String token)
{
    String headers = string_print(arena, "Authorization: Bearer %.*s\r\nAccept: application/json", LIT(token));
    return platform__http_get(arena, url, headers);
}

String_Array get_message_ids(Arena *arena, String token, i64 n)
{
    String_Array result = {0};
    String page_token = S("");
    while (true)
    {
        Platform_HTTP_Response resp = auth_get(arena, sprint("https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=500&includeSpamTrash=1&pageToken=%.*s", LIT(page_token)), token);
        if (resp.status != 200) break;

        D(resp.body);
        JSON_Element *json = json_parse(arena, resp.body);
        JSON_Element *messages = json_find(json, S("messages"));

        for (JSON_EachChild(messages))
        {
            String id = string_push(arena, json_to_string(json_find(it, S("id"))));
            array_push(arena, &result, id);
        }

        page_token = json_to_string(json_find(json, S("nextPageToken")));
        if (!page_token.count || (n > 0 && result.count >= n))
        {
            break;
        }
    }

    if (n > 0 && result.count > n)
    {
        result.count = n;
    }

    return result;
}

void app_run()
{
    Arena *arena = arena_alloc(Gigabytes(1));

    String project_dir = path_dirname(os_get_current_path());
    String secrets_path = path_join(project_dir, S("client_secret.json"));
    String token_path = path_join(project_dir, S("token.json"));

    String contents = os_read_entire_file(arena, secrets_path);
    JSON_Element *root = json_parse(arena, contents);
    JSON_Element *xx = json_find(root, S("installed"));
    D(i64_to_string(json_child_count(xx)));
    String client_id = json_to_string(json_find(xx, S("client_id")));
    String client_secret = json_to_string(json_find(xx, S("client_secret")));
    D(client_id);
    D(client_secret);

    String token_json = S("");
    if (os_file_exists(token_path))
    {
        token_json = os_read_entire_file(arena, token_path);
        #if 0
        token_json = platform__refresh_google_token(arena, client_id, client_secret, token_json);
        D(token_json);
        if (token_json.count)
        {
            os_write_entire_file(token_path, token_json);
            print("Refreshed token!\n");
        }
        #endif
    }
    if (!token_json.count)
    {
        token_json = platform__get_google_token(arena, client_id, client_secret);
        os_write_entire_file(token_path, token_json);
    }

    D(token_json);

    String token = S("");
    {
        JSON_Element *root = json_parse(arena, token_json);
        token = json_to_string(json_find(root, S("access_token")));
    }
    D(token);

    Platform_HTTP_Response resp = auth_get(arena, S("https://gmail.googleapis.com/gmail/v1/users/me/labels"), token);
    if (resp.status == 401)
    {
        os_delete_file(token_path);
        print("Try again.\n");
        return;
    }
    // D(resp.body);

    String_Array ids = get_message_ids(arena, token, 10);
    for (i64 index = 0; index < ids.count; index += 1)
    {
        String id = ids.data[index];
        D(id);
    }

    {
        String host = S("");

        String headers = string_print(arena, "Authorization: Bearer %.*s\r\nAccept: application/json\r\nContent-Type: multipart/mixed; boundary=\"batch_boundary\"", LIT(token));
        String_Array bodies = {0};
        for (i64 index = 0; index < ids.count; index += 1)
        {
            String it = ids.data[index];
            String str = sprint("--batch_boundary\r\n\r\nGET /gmail/v1/users/me/messages/%.*s?format=full\r\n", LIT(it), LIT(it));
            array_push(arena, &bodies, str);
        }
        array_push(arena, &bodies, S("\r\n--batch_boundary--"));

        String body = string_concat_array(arena, bodies.data, bodies.count);
        Platform_HTTP_Response resp = platform__http_post(arena, S("https://www.googleapis.com/batch/gmail/v1"), body, headers);
        D(resp.body);
    }
}