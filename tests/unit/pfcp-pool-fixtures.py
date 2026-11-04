#!/usr/bin/env python3
"""Independent TS 29.244 V19.5.0 wire fixtures; no Open5GS encoder imports.

Write the checked-in C header and a raw-IP PCAP into a supplied directory.
Numeric IE types and lengths here deliberately do not use generated constants.
"""
import argparse
from pathlib import Path
import struct


def ie(kind, value):
    return struct.pack('!HH', kind, len(value)) + value


def identity(value):
    return ie(177, struct.pack('!H', len(value)) + value)


def fixtures():
    pool = (identity(b'edge1-v4-a') + identity(b'A\0B') +
            ie(22, b'\x05edge1') + ie(257, bytes.fromhex('01112233')) +
            ie(257, bytes.fromhex('02ffffff')) + ie(258, b'\x03'))
    second = identity(b'internet') + ie(22, b'\x08internet') + ie(258, b'\x01')
    node = ie(60, bytes.fromhex('007f000008'))
    common = ie(96, bytes.fromhex('01020304'))
    # More feature octets than the baseline emits, including future/spare bits.
    common += ie(43, bytes.fromhex('100000000000000000000080'))
    pools = ie(233, pool) + ie(233, second)
    result = {}
    for name, kind in [('request', 5), ('response', 6)]:
        body = node + (ie(19, b'\x01') if kind == 6 else b'') + common + pools
        result[name] = struct.pack('!BBH', 0x20, kind, len(body)+4) + b'\x00\x00\x01\x00' + body
    return result


def ipv4_udp(payload):
    udp = struct.pack('!HHHH', 8805, 8805, len(payload)+8, 0) + payload
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, len(udp)+20, 1, 0, 64, 17, 0,
                     bytes([127, 0, 0, 8]), bytes([127, 0, 0, 4]))
    checksum = sum(struct.unpack('!10H', ip))
    while checksum >> 16:
        checksum = (checksum & 65535) + (checksum >> 16)
    ip = ip[:10] + struct.pack('!H', checksum ^ 65535) + ip[12:]
    return ip + udp


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    header = '/* Independent wire fixtures; regenerate with pfcp-pool-fixtures.py. */\n'
    pcap = struct.pack('<IHHIIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 101)
    for index, (name, value) in enumerate(fixtures().items()):
        header += f'static const uint8_t pool_{name}_fixture[] = {{\n'
        for offset in range(0, len(value), 12):
            header += '    ' + ', '.join(f'0x{x:02x}' for x in value[offset:offset+12]) + ',\n'
        header += '};\n'
        (args.directory / f'pool-{name}.hex').write_text(value.hex(' ') + '\n')
        packet = ipv4_udp(value)
        pcap += struct.pack('<IIII', 1700000000+index, 0, len(packet), len(packet)) + packet
    (args.directory / 'pfcp-pool-fixtures.h').write_text(header)
    (args.directory / 'pool-setup.pcap').write_bytes(pcap)


if __name__ == '__main__':
    main()
