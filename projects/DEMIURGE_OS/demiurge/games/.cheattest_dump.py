import socket, sys
def cmd(s):
    sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sk.settimeout(2)
    sk.sendto(s.encode(), ("127.0.0.1", 55355))
    try:
        return sk.recvfrom(8192)[0].decode()
    except Exception:
        return ""
    finally:
        sk.close()

def dump(size, outpath):
    buf = bytearray()
    for off in range(0, size, 256):
        n = min(256, size - off)
        r = cmd(f"READ_CORE_RAM {off} {n}").split()
        vals = r[2:]
        if len(vals) != n:
            print(f"short read at {off}: got {len(vals)}");
        buf += bytes(int(x) & 0xff for x in vals[:n])
    open(outpath, "wb").write(buf)
    print("len", len(buf), "->", outpath)

if __name__ == "__main__":
    size = int(sys.argv[1]) if len(sys.argv) > 1 else 5120
    out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/ramdump.bin"
    dump(size, out)
