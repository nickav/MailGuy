static i64 batch_size = 25;

void D_(String label, String x)
{
    print("%.*s = %.*s\n", LIT(label), LIT(x));
}

#define D(x) D_(string_from_cstr(#x), (x))

function String_Array json_to_string_array(JSON_Element *element)
{
    String_Array result = {0};
    if (element)
    {
        for (JSON_EachChild(element))
        {
            array_push(temp_arena(), &result, json_to_string(it));
        }
    }
    return result;
}

function String_Array json_get_String_Array(JSON_Element *object, String child_name)
{
    return json_to_string_array(json_find(object, child_name));
}

b32 string_array_contains(String_Array arr, String x)
{
    for (Array_Each(String, it, arr))
    {
        if (string_equals(*it, x)) return true;
    }
    return false;
}

bool os_make_directory_recursive(String path)
{
    for (i64 i = 1; i < path.count; i += 1)
    {
        if (char_is_slash(path.data[i]))
        {
            String s = string_slice(path, 0, i);
            if (!os_file_exists(s))
            {
                if (!os_make_directory(s))
                {
                    return false;
                }
            }
        }
    }

    if (!os_file_exists(path))
    {
        return os_make_directory(path);
    }

    return true;
}

Platform_HTTP_Response auth_get(Arena *arena, String url, String token)
{
    String headers = string_print(arena, "Authorization: Bearer %.*s\r\nAccept: application/json", LIT(token));
    return platform__http_get(arena, url, headers);
}

String_Array fetch_message_ids(Arena *arena, String token, i64 n)
{
    String_Array result = {0};
    String page_token = S("");
    while (true)
    {
        Platform_HTTP_Response resp = auth_get(arena, sprint("https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=500&includeSpamTrash=1&pageToken=%.*s", LIT(page_token)), token);
        if (resp.status != 200) break;

        JSON_Element *json = json_parse(arena, resp.body);
        if (!json) break;
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

// bool get_new_messages(String last_message_id)
// {
//     https://gmail.googleapis.com/gmail/v1/users/me/history?startHistoryId=12345
// }

// JSON_Element *get_user_profile(String token)
// {
//     Platform_HTTP_Response resp = auth_get(arena, sprint("https://gmail.googleapis.com/gmail/v1/users/me/profile", token);
// }
/*
{
  "emailAddress": "user@gmail.com",
  "messagesTotal": 1234,
  "threadsTotal": 567,
  "historyId": "12345"
}
*/

String get_body_content(Arena *arena, JSON_Element *payload, String desired_mimeType)
{
    if (payload)
    {
        String mimeType = json_get(String, payload, S("mimeType"));
        if (string_equals(mimeType, desired_mimeType))
        {
            String data = json_to_string(json_find2(payload, S("body"), S("data")));
            if (data.count > 0)
            {
                return base64_decode(arena, data);
            }
        }

        JSON_Element *parts = json_find(payload, S("parts"));
        if (parts)
        {
            for (JSON_EachChild(parts))
            {
                String mimeType = json_get(String, it, S("mimeType"));
                if (string_equals(mimeType, desired_mimeType))
                {
                    String data = json_to_string(json_find2(it, S("body"), S("data")));
                    if (data.count > 0)
                    {
                        return base64_decode(arena, data);
                    }
                }
            }
        }
    }

    return S("");
}

String get_body_html(Arena *arena, JSON_Element *payload)
{
    return get_body_content(arena, payload, S("text/html"));
}

String get_body_text(Arena *arena, JSON_Element *payload)
{
    return get_body_content(arena, payload, S("text/plain"));
}

String json_get_header_value(JSON_Element *arr, String key)
{
    String result = {0};
    if (arr)
    {
        for (JSON_EachChild(arr))
        {
            String name = json_get(String, it, S("name"));
            if (string_equals(name, key))
            {
                String value = json_get(String, it, S("value"));
                result = value;
                break;
            }
        }
    }
    return result;
}

void process_message(Arena *arena, String json, String email_dir)
{
    JSON_Element *root = json_parse(arena, json);
    if (!root) return;

    String id = json_get(String, root, S("id"));
    if (!id.count)
    {
        D(json);
        return;
    }
    String tid = json_get(String, root, S("threadId"));
    String snippet = json_get(String, root, S("snippet"));
    String_Array label_ids = json_get(String_Array, root, S("labelIds"));

    JSON_Element *payload = json_find(root, S("payload"));
    JSON_Element *headers = json_find(payload, S("headers"));

    String text = get_body_text(arena, payload);
    String html = get_body_html(arena, payload);

    String_Builder sb = {0};
    sb_print(arena, &sb, "---\n");
    sb_print(arena, &sb, "id: %.*s\n", LIT(id));
    sb_print(arena, &sb, "thread_id: %.*s\n", LIT(tid));

    String keys[] = {
        S("Subject"),
        S("From"),
        S("To"),
        S("Cc"),
        S("Bcc"),
        S("Date"),
        S("Reply-To"),
    };

    for (int i = 0; i < count_of(keys); i += 1)
    {
        String k = keys[i];
        String v = json_get_header_value(headers, k);
        sb_print(arena, &sb, "%.*s: %.*s\n", LIT(k), LIT(v));
    }
    
    sb_print(arena, &sb, "snippet: %.*s\n", LIT(tid));
    sb_print(arena, &sb, "is_read: %d\n", !string_array_contains(label_ids, S("UNREAD")));
    sb_print(arena, &sb, "is_starred: %d\n", string_array_contains(label_ids, S("STARRED")));
    sb_print(arena, &sb, "is_draft: %d\n", string_array_contains(label_ids, S("DRAFT")));
    // sb_print(arena, &sb, "raw_headers: %.*s\n", LIT(tid));
    sb_print(arena, &sb, "---\n");
    sb_print(arena, &sb, "\n\n");

    sb_push(arena, &sb, text);
    sb_push(arena, &sb, S("\n\n<!--html\n"));
    sb_push(arena, &sb, html);
    sb_push(arena, &sb, S("\n-->\n"));

    String name = sprint("%.*s.md", LIT(id));
    String contents = sb_to_string(arena, sb);
    os_write_entire_file(path_join(email_dir, name), contents);
}

void bulk_fetch_messages(Arena *arena, String_Array ids, String token, String email_dir)
{
    assert(ids.count <= 100);
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


    String at = resp.body;
    // NOTE(nick): skip first part because it's the "container"
    String part = string_split_iter(&at, S("HTTP/1.1 "));

    i64 index = 0;
    while (part = string_split_iter(&at, S("HTTP/1.1 ")), part.count > 0)
    {
        String s = string_slice(part, 0, string_index(part, S(" "), 0));
        i64 status = string_to_i64(s, 10);

        while (part.count > 0 && part.data[0] != '{') { string_advance(&part, 1); }
        while (part.count > 0 && part.data[part.count -1] != '}') { part.count -= 1; }

        if (part.count > 0)
        {
            if (status == 200)
            {
                process_message(arena, part, email_dir);
            }
            else
            {
                String id = ids.data[index];
                print("Failed to fetch message id '%.*s' status %d\n", LIT(id), status);
            }
        }

        index += 1;
    }
}

void app_run()
{
    Arena *arena = arena_alloc(Gigabytes(1));

    String exe_dir = os_get_current_path();
    String project_dir = path_dirname(exe_dir);
    String email_dir = path_join(project_dir, S("emails"));
    assert(os_make_directory_recursive(email_dir));

    String secrets_path = path_join(project_dir, S("client_secret.json"));
    String token_path = path_join(project_dir, S("token.json"));

    if (!os_file_exists(secrets_path))
    {
        print("Missing client_secret.json! Expected at: %.*s\n", LIT(secrets_path));
        return;
    }

    String contents = os_read_entire_file(arena, secrets_path);
    JSON_Element *root = json_parse(arena, contents);
    if (!root)
    {
        print("Failed to parse JSON\n");
        return;
    }

    JSON_Element *xx = json_find(root, S("installed"));
    D(i64_to_string(json_child_count(xx)));
    String client_id = json_get(String, xx, S("client_id"));
    String client_secret = json_get(String, xx, S("client_secret"));
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
        if (root)
        {
            token = json_to_string(json_find(root, S("access_token")));
        }
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

    String_Array ids = fetch_message_ids(arena, token, batch_size);

    for (i64 i = 0; i < ids.count; i += batch_size)
    {
        String_Array chunk = array_slice(String_Array, ids, i, i + batch_size);
        bulk_fetch_messages(arena, chunk, token, email_dir);

        if (i+batch_size < ids.count)
        {
            os_sleep(5 / 1000.0);
        }
    }
}