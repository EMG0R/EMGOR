import socket, struct, time

def enc(name):
    out = b''
    for part in name.split('.'):
        out += bytes([len(part)]) + part.encode()
    return out + b'\x00'

header = struct.pack('>HHHHHH', 0, 0, 1, 0, 0, 0)
q = enc('demiurge.local') + struct.pack('>HH', 1, 1)
pkt = header + q
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
s.bind(('0.0.0.0', 0))
s.settimeout(5)
s.sendto(pkt, ('224.0.0.251', 5353))
found = set()
t = time.time()
while time.time() - t < 5:
    try:
        data, addr = s.recvfrom(4096)
    except socket.timeout:
        break
    if b'demiurge' in data:
        idx = 0
        while True:
            j = data.find(b'\x00\x01\x00\x01', idx)
            if j < 0:
                break
            seg = data[j + 4:j + 10]
            if len(seg) == 6 and seg[4:6] == b'\x00\x04':
                ip = data[j + 10:j + 14]
                if len(ip) == 4 and ip[0] != 0:
                    found.add('.'.join(str(b) for b in ip))
            idx = j + 4
for ip in sorted(found):
    print(ip)
