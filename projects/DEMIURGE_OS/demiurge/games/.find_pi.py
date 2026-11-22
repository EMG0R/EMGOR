import socket, concurrent.futures, sys

SUBNET = "10.238.189."

def check(i):
    ip = SUBNET + str(i)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.8)
    try:
        if s.connect_ex((ip, 22)) == 0:
            try:
                banner = s.recv(80).decode(errors="replace").strip()
            except Exception:
                banner = ""
            return (ip, banner)
    except Exception:
        pass
    finally:
        s.close()
    return None

hits = []
with concurrent.futures.ThreadPoolExecutor(max_workers=128) as ex:
    for r in ex.map(check, range(1, 255)):
        if r:
            hits.append(r)
for ip, b in hits:
    print(ip, b)
sys.exit(0 if hits else 1)
