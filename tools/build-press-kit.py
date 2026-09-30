#!/usr/bin/env python3
"""Build epk/emgor-press-kit.zip: photos placed on the EPK page + all songs as 320k mp3 + bio.txt.

    python3 tools/build-press-kit.py            # also called by tools/epk-dev.py on ✓ done

mp3 renders are cached in resources/epk/mp3/ (re-rendered only when the source is newer). Needs ffmpeg.
"""
import json, os, subprocess, sys, zipfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'resources', 'epk')
MP3 = os.path.join(SRC, 'mp3')
ZIP = os.path.join(ROOT, 'epk', 'emgor-press-kit.zip')
LAYOUT = os.path.join(ROOT, 'epk', 'layout.json')

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
    photos = [frames[k]['file'] for k in ('hero', 'a', 'b') if frames.get(k, {}).get('file')]
    mp3s = render_mp3s()
    with zipfile.ZipFile(ZIP, 'w', zipfile.ZIP_DEFLATED) as z:
        for i, rel in enumerate(photos):
            p = os.path.join(ROOT, rel)
            if os.path.exists(p):
                z.write(p, 'EMGOR press kit/photos/emgor-%02d%s' % (i + 1, os.path.splitext(rel)[1]))
        for m in mp3s:
            z.write(m, 'EMGOR press kit/music/' + os.path.basename(m))
        z.writestr('EMGOR press kit/bio.txt', (bio or '').strip() + '\n')
        z.writestr('EMGOR press kit/links.txt', 'EMGOR - Emory Smith\nemgor.online/epk.html\ninstagram.com/_emgor_\ngithub.com/EMG0R\nemorysmith02@gmail.com\n')
    print('wrote', os.path.relpath(ZIP, ROOT), '%.1f MB' % (os.path.getsize(ZIP) / 1e6), '| photos:', len(photos), 'songs:', len(mp3s))

if __name__ == '__main__':
    build(open(sys.argv[1]).read() if len(sys.argv) > 1 else '')
