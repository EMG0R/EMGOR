#!/usr/bin/env python3
"""Local dev server for emgor.online with the APK photo editor's save endpoints.

    python3 tools/apk-dev.py            # serves the repo on http://localhost:8777

Endpoints (editor only works on localhost):
  POST /__apk/upload   {name, data (base64 dataURL)}  -> writes apk/photos/<id>.jpg (max 2000px, q88), returns {path,w,h}
  POST /__apk/draft    {layout}                        -> writes apk/layout.draft.json (work in progress, gitignored)
  POST /__apk/publish  {layout}                        -> writes apk/layout.json + rebuilds apk/emgor-photos.zip
  GET  /__apk/draft                                    -> current draft or {} if none
Never deletes photos: unused ones stay in apk/photos/ until you remove them yourself.
"""
import base64, io, json, os, sys, time, zipfile, hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PHOTOS = os.path.join(ROOT, 'apk', 'photos')
LAYOUT = os.path.join(ROOT, 'apk', 'layout.json')
DRAFT = os.path.join(ROOT, 'apk', 'layout.draft.json')
ZIP = os.path.join(ROOT, 'apk', 'emgor-photos.zip')
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8777

def resize_jpeg(raw):
    try:
        from PIL import Image, ImageOps
    except ImportError:
        return raw, None, None, 'bin'
    im = Image.open(io.BytesIO(raw))
    im = ImageOps.exif_transpose(im)
    if im.mode not in ('RGB', 'L'):
        im = im.convert('RGB')
    im.thumbnail((2000, 2000))
    out = io.BytesIO()
    im.save(out, 'JPEG', quality=88, optimize=True, progressive=True)
    return out.getvalue(), im.width, im.height, 'jpg'

def rebuild_zip(layout):
    used = [s['file'] for s in layout.get('slots', []) if s.get('file')]
    with zipfile.ZipFile(ZIP, 'w', zipfile.ZIP_DEFLATED) as z:
        for i, rel in enumerate(used):
            p = os.path.join(ROOT, rel)
            if os.path.exists(p):
                z.write(p, 'emgor-%02d%s' % (i + 1, os.path.splitext(rel)[1]))

class H(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)
    def log_message(self, fmt, *args):
        if '__apk' in (args[0] if args else ''):
            sys.stderr.write('%s %s\n' % (time.strftime('%H:%M:%S'), fmt % args))
    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()
    def do_GET(self):
        if self.path.split('?')[0] == '/__apk/draft':
            if os.path.exists(DRAFT):
                with open(DRAFT) as f:
                    return self._json(200, json.load(f))
            return self._json(200, {})
        return super().do_GET()
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0))
        try:
            req = json.loads(self.rfile.read(n) or b'{}')
        except Exception as e:
            return self._json(400, {'error': str(e)})
        path = self.path.split('?')[0]
        if path == '/__apk/upload':
            data = req.get('data', '')
            if ',' in data:
                data = data.split(',', 1)[1]
            raw = base64.b64decode(data)
            os.makedirs(PHOTOS, exist_ok=True)
            out, w, h, ext = resize_jpeg(raw)
            pid = hashlib.sha1(out).hexdigest()[:10]
            fn = '%s.%s' % (pid, ext)
            with open(os.path.join(PHOTOS, fn), 'wb') as f:
                f.write(out)
            return self._json(200, {'path': 'apk/photos/' + fn, 'w': w, 'h': h})
        if path == '/__apk/draft':
            with open(DRAFT, 'w') as f:
                json.dump(req.get('layout', {}), f, indent=2)
            return self._json(200, {'ok': True})
        if path == '/__apk/publish':
            layout = req.get('layout', {})
            with open(LAYOUT, 'w') as f:
                json.dump(layout, f, indent=2)
            if os.path.exists(DRAFT):
                os.remove(DRAFT)
            rebuild_zip(layout)
            return self._json(200, {'ok': True})
        return self._json(404, {'error': 'unknown endpoint'})

if __name__ == '__main__':
    print('emgor dev server  http://localhost:%d/apk.html   (ctrl-c to stop)' % PORT)
    ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
