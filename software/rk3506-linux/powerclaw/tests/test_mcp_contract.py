#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


ALLOWED_PATHS = {
    "/api/v1/status",
    "/api/v1/mqtt/status",
    "/api/v1/amp/status",
    "/api/v1/capabilities",
}


class DeviceCoreHandler(BaseHTTPRequestHandler):
    paths = []

    def do_GET(self):
        self.paths.append(self.path)
        if self.path not in ALLOWED_PATHS:
            self.send_response(404)
            self.end_headers()
            return
        payload = json.dumps({"api_version": "1", "test_path": self.path}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *_args):
        pass


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def exchange(process, message):
    process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
    process.stdin.flush()
    line = process.stdout.readline()
    check(line, f"MCP server closed while handling {message['method']}")
    return json.loads(line)


def main():
    binary = os.path.abspath(sys.argv[1])
    server = ThreadingHTTPServer(("127.0.0.1", 18080), DeviceCoreHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    process = subprocess.Popen(
        [binary],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    try:
        initialized = exchange(process, {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "test", "version": "1"},
            },
        })
        check(initialized["result"]["serverInfo"]["name"] == "powerclaw-device",
              "wrong MCP server identity")

        process.stdin.write('{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
        process.stdin.flush()

        listed = exchange(process, {
            "jsonrpc": "2.0", "id": "list-1", "method": "tools/list", "params": {}
        })
        tools = listed["result"]["tools"]
        names = {tool["name"] for tool in tools}
        check(names == {"system_status", "mqtt_status", "amp_status", "capabilities"},
              f"unexpected tools: {names}")
        check(all(tool["inputSchema"].get("additionalProperties") is False for tool in tools),
              "tool schemas must reject arguments")

        expected_paths = {
            "system_status": "/api/v1/status",
            "mqtt_status": "/api/v1/mqtt/status",
            "amp_status": "/api/v1/amp/status",
            "capabilities": "/api/v1/capabilities",
        }
        for index, (name, path) in enumerate(expected_paths.items(), 10):
            response = exchange(process, {
                "jsonrpc": "2.0",
                "id": index,
                "method": "tools/call",
                "params": {"name": name, "arguments": {}},
            })
            check(response["result"]["isError"] is False, f"{name} failed")
            payload = json.loads(response["result"]["content"][0]["text"])
            check(payload["test_path"] == path, f"{name} reached wrong path")

        denied = exchange(process, {
            "jsonrpc": "2.0", "id": 20, "method": "tools/call",
            "params": {"name": "power.control", "arguments": {}},
        })
        check(denied["result"]["isError"] is True, "write-like tool name was accepted")

        injected = exchange(process, {
            "jsonrpc": "2.0", "id": 21, "method": "tools/call",
            "params": {
                "name": "system_status",
                "arguments": {"host": "192.168.1.1", "path": "/admin"},
            },
        })
        check(injected["result"]["isError"] is True, "tool arguments were accepted")
        check(DeviceCoreHandler.paths == list(expected_paths.values()),
              f"unexpected HTTP requests: {DeviceCoreHandler.paths}")
        print("powerclaw MCP contract tests: PASS")
    finally:
        process.terminate()
        process.wait(timeout=2)
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
