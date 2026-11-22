import socket, concurrent.futures

SUBNET = "10.238.189."

def check(i):
    ip = SUBNET + str(i)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.6)
    try:
        if s.connect_ex((ip, 22)) == 0:
            try:
                banner = s.recv(64).decode(errors="replace").strip()
            except Exception:
                banner = ""
            return (ip, banner)
    except Exception:
        pass
    finally:
        s.close()
    return None

with concurrent.futures.ThreadPoolExecutor(max_workers=128) as ex:
    for r in ex.map(check, range(1, 255)):
        if r:
            print(r[0], r[1])
