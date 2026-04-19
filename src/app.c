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

b32 string_array_any_includes(String_Array arr, String x)
{
    for (Array_Each(String, it, arr))
    {
        if (string_includes(*it, x)) return true;
    }
    return false;
}

// returns 1-12 if valid or 0 otherwise
u8 date_month_from_string(String mon)
{
    String months[] = {
        S(""),
        S("Jan"),
        S("Feb"),
        S("Mar"),
        S("Apr"),
        S("May"),
        S("Jun"),
        S("Jul"),
        S("Aug"),
        S("Sep"),
        S("Oct"),
        S("Nov"),
        S("Dec"),
    };

    mon = string_slice(mon, 0, 3);

    for (int i = 1; i < count_of(months); i++)
    {
        String it = months[i];
        if (string_match(it, mon, MatchFlag_IgnoreCase))
        {
            return i;
        }
    }
    return 0;
}

Date_Time parse_email_date(String date)
{
    D(date);
    Date_Time result = {0};

    // Skip optional day of week
    i64 comma = string_index(date, S(","), 0);
    if (comma >= 0)
    {
        date = string_skip(date, comma+1);
    }

    // Skip optional TZ label
    i64 paren = string_index(date, S("("), 0);
    if (paren >= 0)
    {
        date = string_prefix(date, paren);
    }

    date = string_trim_whitespace(date);

    // Now we should have a string that looks like this:
    // 19 Oct 2011 22:39:55 -0400

    i64 spaces = 0;
    i64 i = 0;
    for (; i < date.count; i += 1)
    {
        if (date.data[i] == ' ')
        {
            spaces += 1;
            if (spaces >= 3) { break; }
        }
    }

    String date_part = string_slice(date, 0, i);

    i += 1;
    i64 start = i;
    for (; i < date.count; i += 1)
    {
        if (date.data[i] == ' ') { break; }
    }
    String time_part = string_slice(date, start, i);

    i64 last_space = i;
    for (; i < date.count; i += 1)
    {
        if (date.data[i] == ' ') { last_space = i; }
    }
    String tz_part = string_slice(date, last_space+1, date.count);

    // D(date_part); D(time_part); D(tz_part);

    // date part
    {
        i64 s1 = string_find(date_part, S(" "), 0, 0);
        i64 s2 = string_find(date_part, S(" "), s1+1, 0);

        String day = string_slice(date_part, 0, s1);
        String mon = string_slice(date_part, s1+1, s2);
        String year = string_slice(date_part, s2+1, date_part.count);
        // D(day); D(mon); D(year);

        result.day = string_to_i64(day, 10);
        result.mon = date_month_from_string(mon);
        result.year = string_to_i64(year, 10);
        // D(i64_to_string(result.day)); D(i64_to_string(result.mon)); D(i64_to_string(result.year));
    }

    // time part
    {
        i64 c1 = string_find(time_part, S(":"), 0, 0);
        i64 c2 = string_find(time_part, S(":"), c1+1, 0);

        String hh = string_slice(time_part, 0, c1);
        String mm = string_slice(time_part, c1+1, c2);
        String ss = string_slice(time_part, c2+1, time_part.count);

        result.hour = string_to_i64(hh, 10);
        result.min = string_to_i64(mm, 10);
        result.sec = string_to_i64(ss, 10);
        // D(i64_to_string(result.hour)); D(i64_to_string(result.min)); D(i64_to_string(result.sec));
    }

    // tz part
    {
    }

    // @Incomplete: convert date time to dense time, then apply tz adjustment, then convert back

    return result;
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
        i64 count = n > 0 ? Min(n, 500) : 500;
        Platform_HTTP_Response resp = auth_get(arena, sprint("https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=%d&includeSpamTrash=1&pageToken=%.*s", count, LIT(page_token)), token);
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

JSON_Element *fetch_user_profile(Arena *arena, String token)
{
    Platform_HTTP_Response resp = auth_get(arena, S("https://gmail.googleapis.com/gmail/v1/users/me/profile"), token);
    D(resp.body);
    if (resp.status != 200) return 0;
    return json_parse(arena, resp.body);
}

void fetch_history(Arena *arena, String history_id, String token)
{
    String url = sprint("https://gmail.googleapis.com/gmail/v1/users/me/history?startHistoryId=%.*s&historyTypes=messageAdded&historyTypes=messageDeleted", LIT(history_id));
    Platform_HTTP_Response resp = auth_get(arena, url, token);
    D(resp.body);
    D(i64_to_string(resp.status));
}

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

    String date = json_get_header_value(headers, S("Date"));
    parse_email_date(date);

    for (int i = 0; i < count_of(keys); i += 1)
    {
        String k = keys[i];
        String v = json_get_header_value(headers, k);
        sb_print(arena, &sb, "%.*s: %.*s\n", LIT(k), LIT(v));
    }

    String raw_labels = string_join(arena, label_ids, S(", "));
    String raw_headers = json_stringify(arena, headers);
    
    sb_print(arena, &sb, "snippet: %.*s\n", LIT(snippet));
    sb_print(arena, &sb, "is_read: %d\n", !string_array_contains(label_ids, S("UNREAD")));
    sb_print(arena, &sb, "is_starred: %d\n", string_array_contains(label_ids, S("STARRED")));
    sb_print(arena, &sb, "is_draft: %d\n", string_array_contains(label_ids, S("DRAFT")));
    sb_print(arena, &sb, "headers: %.*s\n", LIT(raw_headers));
    sb_print(arena, &sb, "labels: %.*s\n", LIT(raw_labels));
    sb_print(arena, &sb, "---\n");
    sb_print(arena, &sb, "\n\n");

    sb_push(arena, &sb, text);
    if (html.count > 0)
    {
        sb_push(arena, &sb, S("\n\n<!--html\n"));
        sb_push(arena, &sb, html);
        sb_push(arena, &sb, S("\n-->\n"));
    }

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
    String attachments_dir = path_join(email_dir, S("attachements"));

    assert(os_make_directory_recursive(email_dir));
    assert(os_make_directory_recursive(attachments_dir));

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

    print("Scanning directory...\n");

    String_Array files = {0};
    File_Lister *it = os_file_iter_begin(arena, email_dir);
    File_Info info = {0};
    while (os_file_iter_next(arena, it, &info))
    {
        if (string_starts_with(info.name, S("."))) continue;
        String name = string_push(arena, info.name);
        array_push(arena, &files, name);
    }

    print("Fetching profile...\n");
    JSON_Element *profile = fetch_user_profile(arena, token);

    print("Fetching ids...\n");

    String_Array all_ids = fetch_message_ids(arena, token, 0);
    print("Total id count: %d\n", all_ids.count);

    bool force = false;

    String_Array ids = {0};
    for (i64 i = 0; i < all_ids.count; i += 1)
    {
        String id = all_ids.data[i];
        String name = sprint("%.*s.md", LIT(id));
        if (!string_array_contains(files, name))
        {
            array_push(arena, &ids, id);
        }
    }

    if (force)
    {
        ids = all_ids;
    }

    print("Already cached: %d\n", (all_ids.count - ids.count));
    print("IDs to fetch: %d\n", ids.count);

    for (i64 i = 0; i < ids.count; i += batch_size)
    {
        String_Array chunk = array_slice(String_Array, ids, i, i + batch_size);
        D(i64_to_string(chunk.count));
        bulk_fetch_messages(arena, chunk, token, email_dir);
        print("Fetching chunk (%d, %d)...\n", i, i+batch_size);

        if (i+batch_size < ids.count)
        {
            os_sleep(20 / 1000.0);
        }
    }

    print("Fetched %d emails\n", ids.count);
    print("Done! Took %.2fms\n", os_time() * 1000.0);
}