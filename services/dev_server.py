#!/usr/bin/env python3
import http.server
import socketserver
import os
import argparse

class DualStackServer(http.server.ThreadingHTTPServer):
    def server_bind(self):
        self.socket.setsockopt(http.server.socket.SOL_SOCKET, http.server.socket.SO_REUSEADDR, 1)
        super().server_bind()

class COOPHandler(http.server.SimpleHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_POST(self):
        if self.path in ("/invoke", "/rpc") or self.path.startswith("/invoke?") or self.path.startswith("/rpc?"):
            import urllib.request
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            req = urllib.request.Request(
                f"http://127.0.0.1:8088{self.path}",
                data=body,
                headers={"Content-Type": "application/json"}
            )
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            try:
                with opener.open(req, timeout=5) as resp:
                    resp_data = resp.read()
                    self.send_response(resp.status)
                    self.send_header("Content-Type", "application/json; charset=utf-8")
                    self.send_header("Access-Control-Allow-Origin", "*")
                    self.send_header("Content-Length", str(len(resp_data)))
                    self.end_headers()
                    self.wfile.write(resp_data)
                return
            except Exception as e:
                self.send_response(502)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(b'{"code": -1, "outputs": []}')
                return
        super().do_POST()

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        super().end_headers()

def run(port=8090, directory="."):
    os.chdir(directory)
    handler = COOPHandler
    with DualStackServer(("", port), handler) as httpd:
        print(f"[dev_server] Serving {directory} on http://127.0.0.1:{port}/mindscada.html")
        httpd.serve_forever()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8090)
    parser.add_argument("--dir", type=str, default=".")
    args = parser.parse_args()
    run(args.port, args.dir)
