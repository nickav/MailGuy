#!/usr/bin/env python3

import os
import re
import base64
import subprocess
import argparse
from datetime import datetime, timezone
from email.header import decode_header as _decode_header
from email.utils import parsedate
from concurrent.futures import ThreadPoolExecutor, as_completed

import requests
import html2text

OUTPUT_DIR = "emails"
BATCH_SIZE = 500
MAX_WORKERS = 10
CREDS      = "client_secret.json"
SCOPE      = "https://www.googleapis.com/auth/gmail.readonly"

LABEL_MAP = {
    "INBOX":     "inbox",
    "SPAM":      "spam",
    "TRASH":     "trash",
    "SENT":      "sent",
    "DRAFT":     "drafts",
    "STARRED":   "starred",
    "IMPORTANT": "important",
    "UNREAD":    "unread",
}

from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
import pickle

TOKEN_FILE = "token.pickle"

def get_token():
    creds = None
    if os.path.exists(TOKEN_FILE):
        with open(TOKEN_FILE, "rb") as f:
            creds = pickle.load(f)

    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file(CREDS, [SCOPE])
            creds = flow.run_local_server(port=0)
        with open(TOKEN_FILE, "wb") as f:
            pickle.dump(creds, f)

    return creds.token

def decode_header(value):
    parts  = _decode_header(value)
    result = []
    for part, enc in parts:
        if isinstance(part, bytes):
            result.append(part.decode(enc or "utf-8", errors="replace"))
        else:
            result.append(part)
    return "".join(result)

def format_headers(header_list):
    return [{"name": h["name"], "value": decode_header(h["value"])} for h in header_list]

def to_yaml(value, indent=0):
    pad = "  " * indent
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        if any(c in value for c in ':#{}[]|>&*!,\'"') or value.startswith(' ') or '\n' in value:
            escaped = value.replace('"', '\\"')
            return f'"{escaped}"'
        return value or '""'
    if isinstance(value, list):
        if not value:
            return ""
        lines = []
        for item in value:
            if isinstance(item, (dict, list)):
                lines.append(f"{pad}-\n{to_yaml(item, indent+1)}")
            else:
                lines.append(f"{pad}- {to_yaml(item)}")
        return "\n".join(lines)
    if isinstance(value, dict):
        lines = []
        for k, v in value.items():
            if isinstance(v, (dict, list)):
                lines.append(f"{pad}{k}:\n{to_yaml(v, indent+1)}")
            else:
                lines.append(f"{pad}{k}: {to_yaml(v)}")
        return "\n".join(lines)
    return str(value)

def get_label_map(token):
    headers = {"Authorization": f"Bearer {token}"}
    r       = requests.get(
        "https://gmail.googleapis.com/gmail/v1/users/me/labels",
        headers=headers
    )
    return {l["id"]: l["name"] for l in r.json().get("labels", [])}

def parse_labels(label_ids, label_names):
    folders = []
    labels  = []
    for lid in label_ids:
        if lid in LABEL_MAP:
            folders.append(LABEL_MAP[lid])
        elif lid in label_names:
            labels.append(label_names[lid])
    return folders, labels

#
# for example: https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=1000&labelIds=INBOX&includeSpamTrash=1
# maxResults can be at most 500
#
# labelIds:
# INBOX, SPAM, TRASH, UNREAD, STARRED, IMPORTANT, SENT, DRAFT,
# CATEGORY_PERSONAL, CATEGORY_SOCIAL, CATEGORY_PROMOTIONS, CATEGORY_UPDATES, CATEGORY_FORUMS
#
def get_message_ids(token, n=None):
    ids        = []
    page_token = None
    headers    = {"Authorization": f"Bearer {token}"}

    while True:
        params = {"maxResults": BATCH_SIZE, "includeSpamTrash": "1"}
        if page_token:
            params["pageToken"] = page_token

        r = requests.get(
            "https://gmail.googleapis.com/gmail/v1/users/me/messages",
            headers=headers, params=params
        )
        data = r.json()
        ids += [m["id"] for m in data.get("messages", [])]
        page_token = data.get("nextPageToken")
        if not page_token or (n and len(ids) >= n):
            break

    return ids if not n else ids[:n]

def get_body_text(payload):
    if payload.get("body", {}).get("data"):
        return base64.urlsafe_b64decode(payload["body"]["data"]).decode("utf-8", errors="replace")
    for part in payload.get("parts", []):
        if part["mimeType"] == "text/plain" and part.get("body", {}).get("data"):
            return base64.urlsafe_b64decode(part["body"]["data"]).decode("utf-8", errors="replace")
        if part["mimeType"].startswith("multipart"):
            result = get_body_text(part)
            if result:
                return result
    return ""

def get_body_markdown(payload):
    html = get_body_html(payload)
    if html:
        h = html2text.HTML2Text()
        h.ignore_links = False
        h.ignore_images = False
        h.body_width = 0
        return h.handle(html)
    return get_body_text(payload)

def get_body_html(payload):
    if payload.get("mimeType") == "text/html" and payload.get("body", {}).get("data"):
        return base64.urlsafe_b64decode(payload["body"]["data"]).decode("utf-8", errors="replace")
    for part in payload.get("parts", []):
        if part["mimeType"] == "text/html" and part.get("body", {}).get("data"):
            return base64.urlsafe_b64decode(part["body"]["data"]).decode("utf-8", errors="replace")
        if part["mimeType"].startswith("multipart"):
            result = get_body_html(part)
            if result:
                return result
    return ""

def has_attachments(payload):
    for part in payload.get("parts", []):
        if part.get("filename") and part.get("body", {}).get("attachmentId"):
            return True
        if part["mimeType"].startswith("multipart") and has_attachments(part):
            return True
    return False

def safe_filename(filename):
    return re.sub(r'[^a-zA-Z0-9._-]', '_', filename)

def get_attachments(token, msg_id, payload, outdir):
    attachments = []
    for part in payload.get("parts", []):
        if part["mimeType"].startswith("multipart"):
            attachments += get_attachments(token, msg_id, part, outdir)
            continue

        attach_id = part.get("body", {}).get("attachmentId")
        filename  = part.get("filename", "")
        if not attach_id or not filename:
            continue

        headers = {"Authorization": f"Bearer {token}"}
        r       = requests.get(
            f"https://gmail.googleapis.com/gmail/v1/users/me/messages/{msg_id}/attachments/{attach_id}",
            headers=headers
        )
        data = base64.urlsafe_b64decode(r.json()["data"])

        safe = safe_filename(filename)
        path = os.path.join(outdir, f"{msg_id}_{safe}")
        with open(path, "wb") as f:
            f.write(data)

        attachments.append({
            "filename":  filename,
            "mime_type": part["mimeType"],
            "size":      len(data),
            "saved_as":  path,
        })

    return attachments

def fetch_message(token, msg_id):
    headers = {"Authorization": f"Bearer {token}"}
    r = requests.get(
        f"https://gmail.googleapis.com/gmail/v1/users/me/messages/{msg_id}",
        headers=headers, params={"format": "full"}
    )
    return r.json()

def parse_message(msg, token, outdir, attachments_dir, label_names):
    raw_headers     = format_headers(msg["payload"]["headers"])
    hdrs            = {h["name"]: h["value"] for h in raw_headers}
    label_ids       = msg.get("labelIds", [])
    folders, labels = parse_labels(label_ids, label_names)

    return {
        "id":              msg["id"],
        "thread_id":       msg.get("threadId", ""),
        "message_id":      hdrs.get("Message-ID", ""),
        "in_reply_to":     hdrs.get("In-Reply-To", ""),
        "references":      hdrs.get("References", ""),
        "from":            hdrs.get("From", ""),
        "to":              hdrs.get("To", ""),
        "cc":              hdrs.get("Cc", ""),
        "bcc":             hdrs.get("Bcc", ""),
        "reply_to":        hdrs.get("Reply-To", ""),
        "subject":         hdrs.get("Subject", ""),
        "date":            hdrs.get("Date", ""),
        "internal_date":   int(msg.get("internalDate", 0)),
        "snippet":         msg.get("snippet", ""),
        "size_estimate":   msg.get("sizeEstimate", 0),
        "has_attachments": has_attachments(msg["payload"]),
        "is_read":         "UNREAD" not in label_ids,
        "is_starred":      "STARRED" in label_ids,
        "is_draft":        "DRAFT" in label_ids,
        "folders":         folders,
        "labels":          labels,
        "attachments":     get_attachments(token, msg["id"], msg["payload"], attachments_dir),
        "headers":         raw_headers,
        "body_md":         get_body_markdown(msg["payload"]),
        "body_html":       get_body_html(msg["payload"]),
    }

def make_filename(msg):
    parsed = parsedate(msg.get("date", ""))
    if parsed:
        date_prefix = f"{parsed[0]:04d}-{parsed[1]:02d}-{parsed[2]:02d}"
    else:
        internal = msg.get("internal_date", 0)
        if internal:
            dt          = datetime.fromtimestamp(internal / 1000, tz=timezone.utc)
            date_prefix = dt.strftime("%Y-%m-%d")
        else:
            date_prefix = "0000-00-00"

    subject = msg.get("subject", "")[:32]
    slug    = re.sub(r'[^a-zA-Z0-9]+', '_', subject).strip('_').lower()

    return f"{date_prefix}_{msg['id']}__{slug}.md"

def get_existing_ids(outdir):
    ids = set()
    for f in os.listdir(outdir):
        if not f.endswith(".md"):
            continue

        try:
            # split once on first underscore after date
            rest = f.split("_", 1)[1]           # "{id}__{slug}.md"
            msg_id = rest.split("__", 1)[0]     # "{id}"
            ids.add(msg_id)
        except Exception:
            continue

    return ids

def save_message(msg, outdir):
    body_text = msg.pop("body_md")
    body_html = msg.pop("body_html")

    frontmatter = to_yaml(msg)

    lines = ["---\n", frontmatter, "\n---\n\n"]
    lines.append(body_text or "")
    if body_html:
        lines.append("\n\n<!--html\n\n")
        lines.append(body_html)
        lines.append("\n\n-->\n\n")

    path = os.path.join(outdir, make_filename(msg))
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", type=int, default=None, help="number of emails to fetch (default: all)")
    parser.add_argument("-o", default=OUTPUT_DIR,     help="output directory")
    args = parser.parse_args()

    os.makedirs(args.o, exist_ok=True)
    attachments_dir = os.path.join(args.o, "attachments")
    os.makedirs(attachments_dir, exist_ok=True)

    print("fetching token...")
    token = get_token()

    print("fetching label names...")
    label_names = get_label_map(token)

    print(f"fetching {args.n} message IDs...")
    ids = get_message_ids(token, args.n)
    print(f"got {len(ids)} IDs")

    existing = get_existing_ids(args.o)
    total    = len(ids)
    ids      = [i for i in ids if i not in existing]
    print(f"fetching {len(ids)} new messages ({total - len(ids)} already cached)...")

    done = 0
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
        futures = {ex.submit(fetch_message, token, mid): mid for mid in ids}
        for future in as_completed(futures):
            try:
                msg  = future.result()
                data = parse_message(msg, token, args.o, attachments_dir, label_names)
                save_message(data, args.o)
                done += 1
                if done % 50 == 0:
                    print(f"  {done}/{len(ids)}...")
            except Exception as e:
                print(f"error on {futures[future]}: {e}")

    print(f"done. {done} emails saved to {args.o}/")

if __name__ == "__main__":
    main()