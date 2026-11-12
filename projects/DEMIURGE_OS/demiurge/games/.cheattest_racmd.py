import socket, sys
def cmd(s, host="127.0.0.1", port=55355):
    sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sk.settimeout(2.0)
    sk.sendto(s.encode(), (host, port))
    try:
        data, _ = sk.recvfrom(4096)
        return data.decode(errors="replace").strip()
    except socket.timeout:
        return "<timeout>"
    finally:
        sk.close()
if __name__ == "__main__":
    print(cmd(" ".join(sys.argv[1:])))
