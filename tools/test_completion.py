import socket, json, time

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/Users/Tom/Workspace/meld/.meld/lsp.sock")

def send(obj):
    body = json.dumps(obj)
    s.sendall(("Content-Length: %d\r\n\r\n%s" % (len(body), body)).encode())

def recv(timeout=3):
    s.settimeout(timeout)
    data = b""
    try:
        while True:
            chunk = s.recv(8192)
            if not chunk: break
            data += chunk
    except Exception:
        pass
    return data.decode("utf-8", errors="replace")

fpath = "/Users/Tom/Workspace/meld/meld-examples/examples/29-web-server.meld"
content = open(fpath).read()
uri = "file://" + fpath

send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"processId": None, "rootUri": "file:///Users/Tom/Workspace/meld", "capabilities": {}}})
time.sleep(1)
recv(1)

send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
send({"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": {"uri": uri, "languageId": "meld", "version": 1, "text": content}}})
time.sleep(2)
recv(1)

# Completion at line 44, char 8 (inside "accept-loop" call — should filter to "accept-loop")
send({"jsonrpc": "2.0", "id": 10, "method": "textDocument/completion", "params": {"textDocument": {"uri": uri}, "position": {"line": 43, "character": 8}}})
time.sleep(1)
resp = recv(2)

parts = resp.split("Content-Length: ")
for p in parts[1:]:
    nl = p.find("\r\n\r\n")
    if nl < 0: continue
    ln = int(p[:nl].strip())
    body = p[nl+4:nl+4+ln]
    try:
        j = json.loads(body)
        if j.get("id") == 10:
            r = j.get("result", {})
            items = r.get("items", [])
            print("completions: %d items" % len(items))
            for item in items[:10]:
                print("  %s (%s)" % (item.get("label", "?"), item.get("detail", "")))
    except Exception:
        pass

s.close()
