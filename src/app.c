Struct(Attachment)
{
    String id;
    String name;
    String mime_type;
};

Struct(Attachment_Array)
{
    _ArrayHeader_;
    Attachment *data;
};

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

i32 days_in_month(i32 mon, i32 year)
{
    static const i32 d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon == 2)
    {
        int leap = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
        return 28 + leap;
    }
    return d[mon];
}

void date_time_shift_minutes(Date_Time *dt, int offset_mins)
{
    int total_mins = dt->hour * 60 + dt->min + offset_mins;

    int day_delta = 0;
    if      (total_mins <    0) { total_mins += 1440; day_delta = -1; }
    else if (total_mins >= 1440) { total_mins -= 1440; day_delta = +1; }

    dt->hour = total_mins / 60;
    dt->min  = total_mins % 60;

    int day = dt->day + day_delta;
    int mon = dt->mon;
    int year = dt->year;

    if (day < 1)
    {
        if (--mon < 1) { mon = 12; year--; }
        day = days_in_month(mon, year);
    }
    else if (day > days_in_month(mon, year))
    {
        day = 1;
        if (++mon > 12) { mon = 1; year++; }
    }

    dt->day  = day;
    dt->mon  = mon;
    dt->year = year;
}

void date_time_local_to_utc(Date_Time *date, i32 utc_offset_mins)
{
    date_time_shift_minutes(date, -utc_offset_mins);
}

void date_time_utc_to_local(Date_Time *date, i32 utc_offset_mins)
{
    date_time_shift_minutes(date, utc_offset_mins);
}

Date_Time parse_email_date(String date)
{
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
    // D(date);

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

    // date part (DD MMM YYYY)
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

    // time part (HH:MM:SS)
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

    // tz part (+/- HHMM)
    i64 utc_offset_mins = 0;
    {
        i64 dir = 1;
        if (tz_part.data[0] == '-')
        {
            dir = -1;
            tz_part = string_skip(tz_part, 1);
        }
        if (tz_part.data[0] == '+')
        {
            tz_part = string_skip(tz_part, 1);
        }

        tz_part = string_trim_whitespace(tz_part);

        i64 hh = string_to_i64(string_slice(tz_part, 0, 2), 10);
        i64 mm = string_to_i64(string_slice(tz_part, 2, 4), 10);
        utc_offset_mins = dir * (hh * 60 + mm);
        // D(i64_to_string(utc_offset_mins));
    }

    // Convert to UTC time
    date_time_local_to_utc(&result, utc_offset_mins);

    return result;
}

String date_time_to_sql_date(Date_Time date)
{
    return sprint("%04d-%02d-%02d", date.year, date.mon, date.day);
}

function String time_ago(f64 now, f64 then)
{
    f64 diff = now - then;

    if (diff < 5) return S("just now");
    if (diff < 60) return sprint("%d seconds ago", (i32)diff);
    if (diff < 120) return S("1 minute ago");
    if (diff < 3600) return sprint("%d minutes ago", (i32)(diff / 60));
    if (diff < 7200) return S("1 hour ago");
    if (diff < 86400) return sprint("%d hours ago", (i32)(diff / 3600));
    if (diff < 172800) return S("yesterday");

    return sprint("%d days ago", (i32)(diff / 86400));
}

function String path_sanitize(Arena *arena, String str)
{
    str = string_trim_whitespace(str);

    String result = string_push(arena, str);

    for (i64 i = 0; i < result.count; i++)
    {
        u8 c = result.data[i];
        if (
            c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"'  || c == '<' || c == '>' ||
            c == '|' || c == '\0' || c < 32
        )
        {
            result.data[i] = '_';
        }
    }

    return result;
}

String_Array os_scan_folder(Arena *arena, String folder)
{
    String_Array files = {0};
    File_Lister *it = os_file_iter_begin(arena, folder);
    File_Info info = {0};
    while (os_file_iter_next(arena, it, &info))
    {
        if (string_starts_with(info.name, S("."))) continue;
        String name = string_push(arena, info.name);
        array_push(arena, &files, name);
    }
    return files;
}


HTTP_Response auth_get(Arena *arena, String url, String token)
{
    String headers = string_print(arena, "Authorization: Bearer %.*s\r\nAccept: application/json", LIT(token));
    return platform__http_get(arena, url, headers);
}

HTTP_Response_Array gmail_bulk_fetch(Arena *arena, String token, String_Array requests)
{
    assert(requests.count <= 100);

    HTTP_Response_Array result = {0};
    if (!requests.count) return result;

    String headers = string_print(arena,
        "Authorization: Bearer %.*s\r\nAccept: application/json\r\nContent-Type: multipart/mixed; boundary=\"batch_boundary\"",
        LIT(token));

    String_Array bodies = {0};
    for (i64 i = 0; i < requests.count; i++)
    {
        String str = sprint("--batch_boundary\r\n\r\n%.*s\r\n", LIT(requests.data[i]));
        array_push(arena, &bodies, str);
    }
    array_push(arena, &bodies, S("\r\n--batch_boundary--"));

    String body = string_concat_array(arena, bodies.data, bodies.count);
    HTTP_Response resp = platform__http_post(arena, S("https://www.googleapis.com/batch/gmail/v1"), body, headers);

    String at = resp.body;
    String part = string_split_iter(&at, S("HTTP/1.1 "));

    while ((part = string_split_iter(&at, S("HTTP/1.1 "))), part.count > 0)
    {
        String status_str = string_slice(part, 0, string_index(part, S(" "), 0));

        while (part.count > 0 && part.data[0] != '{') { string_advance(&part, 1); }
        while (part.count > 0 && part.data[part.count-1] != '}') { part.count -= 1; }

        HTTP_Response item = {
            .status = string_to_i64(status_str, 10),
            .body   = string_push(arena, part),
        };
        array_push(arena, &result, item);
    }

    return result;
}

HTTP_Response_Array gmail_bulk_fetch_with_retry(Arena *arena, String token, String_Array requests, i32 max_retries, f64 retry_sleep_secs)
{
    HTTP_Response_Array result = gmail_bulk_fetch(arena, token, requests);

    for (i32 attempt = 0; attempt < max_retries; attempt++)
    {
        String_Array retry_requests = {0};
        Array_i64    retry_indices  = {0};
        for (i64 i = 0; i < result.count; i++)
        {
            if (result.data[i].status != 200)
            {
                array_push(arena, &retry_requests, requests.data[i]);
                array_push(arena, &retry_indices,  i);
            }
        }

        if (!retry_requests.count) break;

        print("Retrying %d failed requests (attempt %d)...\n", retry_requests.count, attempt + 1);
        if (retry_sleep_secs)
        {
            os_sleep(retry_sleep_secs);
        }

        HTTP_Response_Array retried = gmail_bulk_fetch(arena, token, retry_requests);
        for (i64 i = 0; i < retried.count; i++)
        {
            result.data[retry_indices.data[i]] = retried.data[i];
        }
    }

    return result;
}

String_Array fetch_message_ids(Arena *arena, String token, i64 n)
{
    String_Array result = {0};
    String page_token = S("");
    while (true)
    {
        i64 count = n > 0 ? Min(n, 500) : 500;
        HTTP_Response resp = auth_get(arena, sprint("https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=%d&includeSpamTrash=1&pageToken=%.*s", count, LIT(page_token)), token);
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

JSON_Element *fetch_user_profile(Arena *arena, String token)
{
    HTTP_Response resp = auth_get(arena, S("https://gmail.googleapis.com/gmail/v1/users/me/profile"), token);
    D(resp.body);
    if (resp.status != 200) return 0;
    return json_parse(arena, resp.body);
}

void fetch_history(Arena *arena, String history_id, String token)
{
    String url = sprint("https://gmail.googleapis.com/gmail/v1/users/me/history?startHistoryId=%.*s&historyTypes=messageAdded&historyTypes=messageDeleted", LIT(history_id));
    HTTP_Response resp = auth_get(arena, url, token);
    D(resp.body);
    // D(i64_to_string(resp.status));
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

Attachment_Array get_message_attachments(Arena *arena, JSON_Element *payload)
{
    Attachment_Array result = {0};
    if (!payload) return result;

    JSON_Element *parts = json_find(payload, S("parts"));
    if (!parts) return result;

    for (JSON_EachChild(parts))
    {
        String mimeType = json_get(String, it, S("mimeType"));
        if (string_starts_with(mimeType, S("multipart")))
        {
            Attachment_Array children = get_message_attachments(arena, it);
            array_concat(arena, &result, children);
            continue;
        }

        String id = json_to_string(json_find2(it, S("body"), S("attachmentId")));
        String name  = json_get(String, it, S("filename"));
        if (id.count && name.count)
        {
            Attachment info = {
                .id = string_push(arena, id),
                .name  = string_push(arena, name),
                .mime_type = string_push(arena, mimeType),
            };
            array_push(arena, &result, info);
        }
    }

    return result;
}

String get_attachment_path(Arena *arena, String msg_id, Attachment attachment, String attachments_dir)
{
    String safe = path_sanitize(arena, attachment.name);
    return path_join2(arena, attachments_dir, sprint("%.*s_%.*s", LIT(msg_id), LIT(safe)));
}

void bulk_fetch_attachments(Arena *arena, String token, String msg_id, Attachment_Array attachments, String attachments_dir)
{
    String_Array requests = {0};
    for (i64 index = 0; index < attachments.count; index += 1)
    {
        Attachment it = attachments.data[index];
        array_push(arena, &requests, sprint("GET /gmail/v1/users/me/messages/%.*s/attachments/%.*s", LIT(msg_id), LIT(it.id)));
    }

    HTTP_Response_Array responses = gmail_bulk_fetch_with_retry(arena, token, requests, 4, 5.0);
    for (i64 index = 0; index < responses.count; index += 1)
    {
        Attachment it = attachments.data[index];
        HTTP_Response resp = responses.data[index];
        if (resp.status == 200)
        {
            M_Temp temp = arena_begin_temp(arena);
            {
                JSON_Element *json = json_parse(temp.arena, resp.body);
                String encoded = json_get(String, json, S("data"));
                String data    = base64_decode(temp.arena, encoded);

                String attachment_path = get_attachment_path(arena, msg_id, it, attachments_dir);
                os_write_entire_file(attachment_path, data);
            }
            arena_end_temp(temp);
        }
        else
        {
            print("Failed to fetch message attachment id '%.*s' status %d\n", LIT(it.id), resp.status);
        }
    }
}

void save_message(Arena *temp, String json, String email_dir, String token, String attachments_dir)
{
    Arena *arena = temp;

    JSON_Element *root = json_parse(arena, json);
    if (!root) return;
    // D(json);

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

    Attachment_Array attachments = get_message_attachments(arena, payload);
    bulk_fetch_attachments(arena, token, id, attachments, attachments_dir);

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

    String raw_date = json_get_header_value(headers, S("Date"));
    Date_Time parsed_date = parse_email_date(raw_date);
    String date = date_time_to_sql_date(parsed_date);

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
    if (attachments.count > 0)
    {
        sb_print(arena, &sb, "attachements:\n");
        for (i64 index = 0; index < attachments.count; index += 1)
        {
            Attachment it = attachments.data[index];

            String file = get_attachment_path(arena, id, it, attachments_dir);
            sb_print(arena, &sb, "  -\n");
            sb_print(arena, &sb, "    file: %.*s\n", LIT(file));
            sb_print(arena, &sb, "    name: %.*s\n", LIT(it.name));
            sb_print(arena, &sb, "    type: %.*s\n", LIT(it.mime_type));
        }
    }
    sb_print(arena, &sb, "---\n");
    sb_print(arena, &sb, "\n\n");

    sb_push(arena, &sb, text);
    if (html.count > 0)
    {
        sb_push(arena, &sb, S("\n\n<!--html\n"));
        sb_push(arena, &sb, html);
        sb_push(arena, &sb, S("\n-->\n"));
    }

    String name = sprint("%.*s_%.*s.md", LIT(date), LIT(id));
    String contents = sb_to_string(arena, sb);
    os_write_entire_file(path_join(email_dir, name), contents);
}

void bulk_fetch_messages(Arena *arena, String_Array ids, String token, String email_dir, String attachments_dir)
{
    String_Array requests = {0};
    for (i64 index = 0; index < ids.count; index += 1)
    {
        String id = ids.data[index];
        array_push(arena, &requests, sprint("GET /gmail/v1/users/me/messages/%.*s?format=full", LIT(id)));
    }

    HTTP_Response_Array responses = gmail_bulk_fetch_with_retry(arena, token, requests, 4, 5.0);
    assert(responses.count == ids.count);
    for (i64 index = 0; index < responses.count; index += 1)
    {
        HTTP_Response resp = responses.data[index];
        if (resp.status == 200)
        {
            M_Temp temp = arena_begin_temp(arena);
            save_message(arena, resp.body, email_dir, token, attachments_dir);
            arena_end_temp(temp);
        }
        else
        {
            String id = ids.data[index];
            print("Failed to fetch message id '%.*s' status %d\n", LIT(id), resp.status);
        }
    }
}

//
// App
//

static struct {
    Arena *persist_arena;
    Arena *frame_arena;

    String app_data;

    String client_id;
    String client_secret;

    String email_dir;
    String refresh_path;

    // Settings:
    i32 batch_size;
    i32 refresh_interval_mins;
    i32 fetch_count;

    // Running instance:
    String email;
    i64 total_count;
    i64 cached_count;

    // Run info:
    f64 last_run;
    b32 is_running;
} g_app = {
    .batch_size = 25,
    .refresh_interval_mins = 5,
};

void app_run();

void app_menu(i32 x, i32 y)
{
    Arena *arena = temp_arena();

    String last_checked = string_concat(S("Last checked: "), time_ago(os_time(), g_app.last_run));

    MenuItem menu = {0};
    menu_push(arena, &menu, (MenuItem){ .disabled = true, .name = g_app.email });
    menu_push(arena, &menu, (MenuItem){ .separator = true });

    MenuItem *run = menu_push(arena, &menu, (MenuItem){ .id = 1, .name = S("Check Now") });
    if (g_app.is_running) {
        run->name = S("Running...");
        run->disabled = true;
    }

    menu_push(arena, &menu, (MenuItem){ .disabled = true, .name = last_checked });
    menu_push(arena, &menu, (MenuItem){ .disabled = true, .name = sprint("%d / %d downloaded", g_app.cached_count, g_app.total_count) });
    
    MenuItem *refresh_menu = menu_push(arena, &menu, (MenuItem){ .id = 10, .name = S("Refresh Interval") });
        menu_push(arena, refresh_menu, (MenuItem){ .id = 11, .name = S("1 Minute"),   .checked = g_app.refresh_interval_mins == 1 });
        menu_push(arena, refresh_menu, (MenuItem){ .id = 12, .name = S("5 Minutes"),  .checked = g_app.refresh_interval_mins == 5 });
        menu_push(arena, refresh_menu, (MenuItem){ .id = 13, .name = S("10 Minutes"), .checked = g_app.refresh_interval_mins == 10 });
        menu_push(arena, refresh_menu, (MenuItem){ .id = 14, .name = S("15 Minutes"), .checked = g_app.refresh_interval_mins == 15 });

    menu_push(arena, &menu, (MenuItem){ .id = 4, .name = S("Open Folder") });

    bool exists = os_file_exists(g_app.refresh_path);
    if (exists)
    {
        menu_push(arena, &menu, (MenuItem){ .id = 3, .name = S("Sign-out") });
    }

    menu_push(arena, &menu, (MenuItem){ .id = 2, .name = S("Quit") });
    
    i64 result = platform__show_menu(menu.subitems.data, menu.subitems.count, x, y);

    switch (result)
    {
        case 1:
        {
            app_run();
            arena_reset(g_app.frame_arena);
        } break;

        case 2:
        {
            platform__quit();
        } break;

        case 3:
        {
            os_delete_file(g_app.refresh_path);
        } break;

        case 4:
        {
            os_shell_open(g_app.email_dir);
        } break;

        case 11: { g_app.refresh_interval_mins = 1; }
        case 12: { g_app.refresh_interval_mins = 5; }
        case 13: { g_app.refresh_interval_mins = 10; }
        case 14: { g_app.refresh_interval_mins = 15; }
    }
}

void app_init()
{
    g_app.persist_arena = arena_alloc(Gigabytes(1));
    g_app.frame_arena = arena_alloc(Gigabytes(1));

    Arena *arena = g_app.persist_arena;

    String app_data = path_join(os_get_system_path(arena, SystemPath_AppData), S("MailGuy"));
    String exe_dir = os_get_system_path(arena, SystemPath_Binary);

    if (!os_file_exists(app_data))
    {
        assert(os_make_directory(app_data));
    }
    g_app.app_data = string_push(arena, app_data);

    String secrets_path1 = path_join(app_data, S("client_secret.json"));
    String secrets_path2 = path_join(exe_dir, S("client_secret.json"));

    String secrets_path = secrets_path1;
    if (!os_file_exists(secrets_path)) { secrets_path = secrets_path2; }

    // Get secrets
    if (!os_file_exists(secrets_path))
    {
        print("Missing client_secret.json! Looked in:\n");
        print("  - %.*s\n", LIT(secrets_path1));
        print("  - %.*s\n", LIT(secrets_path2));
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
    g_app.client_id = string_push(arena, client_id);
    g_app.client_secret = string_push(arena, client_secret);

    // @Incomplete: make this a setting?
    String project_dir = path_dirname(exe_dir);
    String email_dir = path_join(project_dir, S("emails"));

    String refresh_path = path_join(app_data, S("refresh_token.txt"));

    g_app.email_dir = string_push(arena, email_dir);
    g_app.refresh_path = string_push(arena, refresh_path);
    app_run();
    arena_reset(g_app.frame_arena);
}

void app_run()
{
    if (g_app.is_running)
    {
        return;
    }

    // @Incomplete: support multiple accounts...
    Arena *arena = g_app.frame_arena;
    String parent_email_dir = g_app.email_dir;
    String refresh_path = g_app.refresh_path;

    g_app.is_running = true;


    // Get refresh token if we don't have one
    String refresh_token = S("");
    if (os_file_exists(refresh_path))
    {
        refresh_token = os_read_entire_file(arena, refresh_path);
    }

    String client_id = g_app.client_id;
    String client_secret = g_app.client_secret;

    if (!refresh_token.count)
    {
        String body = platform__get_google_token(arena, client_id, client_secret);
        JSON_Element *root = json_parse(arena, body);
        refresh_token = json_get(String, root, S("refresh_token"));

        if (!refresh_token.count)
        {
            print("Failed to get refresh token!\n");
            D(body);
            g_app.is_running = false;
            return;
        }

        os_write_entire_file(refresh_path, refresh_token);
    }

    D(refresh_token);

    String token_json = platform__refresh_google_token(arena, client_id, client_secret, refresh_token);
    D(token_json);
    if (!token_json.count)
    {
        os_delete_file(refresh_path);
        print("Failed to get auth token using refresh token!\n");
        g_app.is_running = false;
        return;
    }

    String token = S("");
    {
        JSON_Element *root = json_parse(arena, token_json);
        if (root)
        {
            token = json_to_string(json_find(root, S("access_token")));
        }
    }
    D(token);

    print("Fetching profile...\n");
    JSON_Element *profile = fetch_user_profile(arena, token);
    String email = json_get(String, profile, S("emailAddress"));
    if (!string_equals(g_app.email, email))
    {
        g_app.email = string_push(g_app.persist_arena, email);
    }

    #if 0
    HTTP_Response resp = auth_get(arena, S("https://gmail.googleapis.com/gmail/v1/users/me/labels"), token);
    if (resp.status == 401)
    {
        os_delete_file(refresh_path);
        print("Try again.\n");
        g_app.is_running = false;
        return;
    }
    // D(resp.body);
    #endif

    print("Scanning directory...\n");
    String email_dir = path_join2(arena, parent_email_dir, path_sanitize(arena, email));
    String attachments_dir = path_join(email_dir, S("attachements"));

    if (!os_make_directory_recursive(email_dir))
    {
        print("Failed to make email folder: %.*s\n", LIT(email_dir));
        g_app.is_running = false;
        return;
    }
    if (!os_make_directory_recursive(attachments_dir))
    {
        print("Failed to make attachements folder: %.*s\n", LIT(attachments_dir));
        g_app.is_running = false;
        return;
    }

    String_Array files = os_scan_folder(arena, email_dir);
    print("Fetching ids...\n");

    String_Array all_ids = fetch_message_ids(arena, token, g_app.fetch_count);
    print("Total id count: %d\n", all_ids.count);
    g_app.total_count = all_ids.count;

    bool force = false;

    String_Array ids = {0};
    for (i64 i = 0; i < all_ids.count; i += 1)
    {
        String id = all_ids.data[i];
        if (!string_array_any_includes(files, id))
        {
            array_push(arena, &ids, id);
        }
    }
    g_app.cached_count = (all_ids.count - ids.count);

    if (force)
    {
        ids = all_ids;
    }

    print("Already cached: %d\n", (all_ids.count - ids.count));
    print("IDs to fetch: %d\n", ids.count);

    i32 batch_size = g_app.batch_size;
    for (i64 i = 0; i < ids.count; i += batch_size)
    {
        String_Array chunk = array_slice(String_Array, ids, i, i + batch_size);
        // D(i64_to_string(chunk.count));
        bulk_fetch_messages(arena, chunk, token, email_dir, attachments_dir);
        print("Fetching chunk (%d, %d)...\n", i, i+batch_size);

        if (i+batch_size < ids.count)
        {
            os_sleep(20 / 1000.0);
        }
    }

    print("Fetched %d emails\n", ids.count);
    print("Run complete! Took %.2fms\n", os_time() * 1000.0);

    // NOTE(nick); after a fetch, re-scan
    {
        files = os_scan_folder(arena, email_dir);
        ids = (String_Array){0};
        for (i64 i = 0; i < all_ids.count; i += 1)
        {
            String id = all_ids.data[i];
            if (!string_array_any_includes(files, id))
            {
                array_push(arena, &ids, id);
            }
        }
        g_app.cached_count = (all_ids.count - ids.count);
    }

    g_app.last_run = os_time();
    g_app.is_running = false;
}

void app_tick(f32 dt)
{
    static f64 time = 0;
    time += dt;

    // @Hack: until we do a proper history api thing, this should suffice...
    if (time >= 60 * g_app.refresh_interval_mins)
    {
        time -= 60 * g_app.refresh_interval_mins;

        app_run();
        arena_reset(g_app.frame_arena);
    }
}

void app_quit()
{
}