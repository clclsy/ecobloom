#!/usr/bin/env python3
"""
mock_server.py — a throwaway HTTP server that prints whatever JSON the
device POSTs to it. Use this to confirm the Pi/QNX device (or local_test.cpp)
is actually sending readings, before your real website's API is ready.

Run:
    python3 mock_server.py
Then point CarbonEstimatorConfig.reportHost/reportPort at this machine's
IP and 8000.
"""
from http.server import BaseHTTPRequestHandler, HTTPServer
import json

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        try:
            reading = json.loads(body)
            print("Reading received:", json.dumps(reading, indent=2))
        except json.JSONDecodeError:
            print("Non-JSON body received:", body)

        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(b'{"status":"ok"}')

    def log_message(self, format, *args):
        pass  # quiet the default request logging, we print our own

if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 8000), Handler)
    print("Mock server listening on :8000 ...")
    server.serve_forever()
