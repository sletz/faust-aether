"""Serve the local project for AudioWorklet/Web MIDI on localhost."""
import functools
import http.server
from pathlib import Path
import sys

class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,'.js':'text/javascript','.wasm':'application/wasm'}
    def end_headers(self):
        self.send_header('Cache-Control','no-store')
        super().end_headers()

root = Path(__file__).resolve().parents[1]
port = int(sys.argv[1]) if len(sys.argv)>1 else 8088
with http.server.ThreadingHTTPServer(('127.0.0.1',port),functools.partial(Handler,directory=str(root))) as server:
    print(f'Aether : http://127.0.0.1:{port}/web/',flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
