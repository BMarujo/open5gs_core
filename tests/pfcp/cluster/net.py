#!/usr/bin/env python3
"""Configure the lab Pod's existing tunlab only: GATEWAY/PREFIX [UE_IP].
Run with NET_ADMIN in the UPF Pod network namespace. No external N6 routing.
"""
import ipaddress
import socket
import struct
import sys

address = ipaddress.IPv4Interface(sys.argv[1])
index = socket.if_nametoindex('tunlab')


def attr(typ, data):
    n = 4 + len(data)
    return struct.pack('HH', n, typ) + data + b'\0' * ((-n) % 4)


updates = [(16, 5, struct.pack('BBHiII', 0, 0, 0, index, 1, 1))]
body = struct.pack('BBBBI', socket.AF_INET, address.network.prefixlen, 0, 0, index)
body += attr(1, address.ip.packed) + attr(2, address.ip.packed)
updates.append((20, 1 | 4 | 0x400 | 0x200, body))
if len(sys.argv) > 2:
    ue = ipaddress.IPv4Address(sys.argv[2])
    body = struct.pack('BBBBBBBBI', socket.AF_INET, 32, 0, 0, 254, 3, 253, 1, 0)
    body += attr(1, ue.packed) + attr(4, struct.pack('I', index))
    updates.append((24, 1 | 4 | 0x400 | 0x100, body))
with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 0) as sock:
    sock.settimeout(3)
    for typ, flags, payload in updates:
        sock.sendto(struct.pack('IHHII', 16+len(payload), typ, flags, 1, 0) + payload, (0, 0))
        error = struct.unpack_from('i', sock.recv(4096), 16)[0]
        assert error == 0 or (typ == 20 and error == -17), error
print('Configured tunlab', address, 'UE route', sys.argv[2:] or 'connected prefix only')
