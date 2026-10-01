#!/usr/bin/env python3
"""Build epk/emgor-press-kit.zip: photos placed on the EPK page + the songs listed on it as 320k mp3, named as on the page.

    python3 tools/build-press-kit.py            # also called by tools/epk-dev.py on ✓ done

mp3 renders are cached in resources/epk/mp3/ (re-rendered only when the source is newer). Needs ffmpeg.
"""
import json, os, re, subprocess, sys, zipfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'resources', 'epk')
MP3 = os.path.join(SRC, 'mp3')
ZIP = os.path.join(ROOT, 'epk', 'EMGOR press kit.zip')
LAYOUT = os.path.join(ROOT, 'epk', 'layout.json')
PAGE = os.path.join(ROOT, 'epk.html')

def page_tracks():
    """[(display name, mp3 stem)] from the data-tracks JSON on epk.html."""
    html = open(PAGE).read()
    m = re.search(r"data-tracks='(\[.*?\])'", html, re.S)
    tracks = json.loads(m.group(1)) if m else []
    return [(t['name'], os.path.splitext(os.path.basename(t['src']))[0]) for t in tracks]

def page_bio():
    """Plain-text bio from the .bio column on epk.html."""
    html = open(PAGE).read()
    m = re.search(r'<div class="col bio">(.*?)</div>', html, re.S)
    if not m: return ''
    t = m.group(1)
    t = re.sub(r'<h2>.*?</h2>', '', t, flags=re.S)
    t = re.sub(r'<p class="bio-links">.*?</p>', '', t, flags=re.S)
    t = re.sub(r'<li>', '- ', t); t = re.sub(r'</li>|</p>|<br>', '\n', t)
    t = re.sub(r'<span>([^<]*)</span>', r'\1: ', t)
    t = re.sub(r'<[^>]+>', '', t)
    import html as H; t = H.unescape(t)
    t = '\n'.join(line.strip() for line in t.splitlines()); t = re.sub(r'[ \t]+', ' ', t); t = re.sub(r'\n\s*\n+', '\n\n', t)
    return 'EMGOR - Emory Smith\n\n' + t.strip() + '\n\ninstagram: instagram.com/_emgor_\nit is (ep): emgor.online/it-is.html\nemail: emorysmith02@gmail.com\nsite: emgor.online/epk.html\n'

def slug(name):
    """'the new sick (w/ cal digiovanni + omer kochba)' -> 'the_new_sick_w_cal_digiovanni_omer_kochba'"""
    n = name.lower().replace('w/', 'w').replace('+', ' ')
    n = re.sub(r'[^a-z0-9]+', '_', n).strip('_')
    return n

def safe(name):
    return re.sub(r'[\\/:*?"<>|]', '', name).replace('/', '').strip()


def render_mp3s():
    os.makedirs(MP3, exist_ok=True)
    out = []
    for f in sorted(os.listdir(SRC)):
        if not f.lower().endswith(('.wav', '.m4a', '.aif', '.aiff', '.flac')):
            continue
        src = os.path.join(SRC, f)
        dst = os.path.join(MP3, os.path.splitext(f)[0] + '.mp3')
        if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
            subprocess.run(['ffmpeg', '-v', 'quiet', '-y', '-i', src, '-codec:a', 'libmp3lame', '-b:a', '320k', '-id3v2_version', '3',
                            '-metadata', 'artist=EMGOR', '-metadata', 'title=' + os.path.splitext(f)[0].replace('-', ' '), dst], check=True)
            print('rendered', os.path.basename(dst))
        out.append(dst)
    return out

def build(bio=''):
    layout = json.load(open(LAYOUT)) if os.path.exists(LAYOUT) else {}
    frames = layout.get('frames', {})
    photos = [frames[k]['file'] for k in ('a', 'b', 'hero') if frames.get(k, {}).get('file')]
    render_mp3s()
    tracks = page_tracks()
    with zipfile.ZipFile(ZIP, 'w', zipfile.ZIP_DEFLATED) as z:
        for i, rel in enumerate(photos):
            p = os.path.join(ROOT, rel)
            # full-quality original if the uploader kept one, else the web copy
            stem = os.path.splitext(os.path.basename(rel))[0]
            odir = os.path.join(ROOT, 'epk', 'photos', 'originals')
            orig = [os.path.join(odir, f) for f in (os.listdir(odir) if os.path.isdir(odir) else []) if f.startswith(stem + '.')]
            src = orig[0] if orig else p
            if os.path.exists(src):
                z.write(src, 'EMGOR press kit/photos/EMGOR_%d%s' % (i + 1, os.path.splitext(src)[1].lower().replace('.jpeg', '.jpg')))
        z.writestr('EMGOR press kit/README.txt', page_bio())
        for name, stem in tracks:
            m = os.path.join(MP3, stem + '.mp3')
            if os.path.exists(m):
                subprocess.run(['ffmpeg', '-v', 'quiet', '-y', '-i', m, '-codec', 'copy', '-id3v2_version', '3', '-metadata', 'artist=EMGOR', '-metadata', 'title=' + name, m + '.tmp.mp3'])
                src = m + '.tmp.mp3' if os.path.exists(m + '.tmp.mp3') else m
                z.write(src, 'EMGOR press kit/music/' + slug(name) + '.mp3')
                if src != m: os.remove(src)
    print('wrote', os.path.relpath(ZIP, ROOT), '%.1f MB' % (os.path.getsize(ZIP) / 1e6), '| photos:', len(photos), 'songs:', len(tracks))

if __name__ == '__main__':
    build()
