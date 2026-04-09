#!/usr/bin/env python3
"""UV-K5 Firmware Flasher - Compact Version"""

import serial
import serial.tools.list_ports
import time
import struct

BAUDRATE = 38400
MAX_FW_SIZE = 120 * 1024
OBFUS = bytes([0x16, 0x6c, 0x14, 0xe6, 0x2e, 0x91, 0x0d, 0x40,
               0x21, 0x35, 0xd5, 0x40, 0x13, 0x03, 0xe9, 0x80])


class Flasher:
    def __init__(self, port):
        self.ser = serial.Serial(port, BAUDRATE, timeout=5, write_timeout=5)
        self.buf = bytearray()

    def xor(self, data, off, sz):
        for i in range(sz):
            data[off + i] ^= OBFUS[i % 16]

    def crc(self, data, off, sz):
        c = 0
        for i in range(sz):
            c ^= data[off + i] << 8
            for _ in range(8):
                c = (c << 1 ^ 0x1021 if c & 0x8000 else c << 1) & 0xFFFF
        return c

    def send(self, msg):
        ln = len(msg) + (len(msg) % 2)
        pkt = bytearray(8 + ln)
        struct.pack_into('<HH', pkt, 0, 0xCDAB, ln)
        pkt[4:4+len(msg)] = msg
        struct.pack_into('<HH', pkt, 4+ln, self.crc(pkt, 4, ln), 0xBADC)
        self.xor(pkt, 4, 2 + ln)
        self.ser.write(pkt)

    def recv(self):
        if len(self.buf) < 8:
            return None
        
        idx = next((i for i in range(len(self.buf)-1) 
                   if self.buf[i:i+2] == b'\xAB\xCD'), -1)
        if idx == -1:
            self.buf = self.buf[-1:] if self.buf and self.buf[-1] == 0xAB else bytearray()
            return None

        if len(self.buf) - idx < 8:
            return None

        ln = struct.unpack_from('<H', self.buf, idx+2)[0]
        end = idx + 6 + ln

        if len(self.buf) < end + 2 or self.buf[end:end+2] != b'\xDC\xBA':
            del self.buf[:idx+2]
            return None

        msg = bytearray(self.buf[idx+4:idx+4+ln+2])
        self.xor(msg, 0, ln + 2)
        del self.buf[:end+2]
        
        return struct.unpack_from('<H', msg, 0)[0], bytes(msg[4:])

    def wait_dev(self):
        acc, last = 0, 0
        for _ in range(500):
            time.sleep(0.01)
            if self.ser.in_waiting:
                self.buf.extend(self.ser.read(self.ser.in_waiting))
            
            m = self.recv()
            if not m or m[0] != 0x0518:
                continue

            now = time.time()
            if 0.005 <= now - last <= 1 and last:
                acc += 1
                if acc >= 5:
                    return m[1][16:32].split(b'\x00')[0].decode('ascii', errors='ignore')
            else:
                acc = 0
            last = now

        raise TimeoutError("Radio not detected")

    def handshake(self, ver):
        for _ in range(3):
            time.sleep(0.05)
            if self.ser.in_waiting:
                self.buf.extend(self.ser.read(self.ser.in_waiting))
            if (m := self.recv()) and m[0] == 0x0518:
                msg = bytearray(8)
                struct.pack_into('<HH', msg, 0, 0x0530, 4)
                msg[4:8] = ver[:4].encode('ascii')
                self.send(msg)
        
        time.sleep(0.2)
        while self.recv():
            pass

    def flash(self, data):
        pages = (len(data) + 255) // 256
        ts = int(time.time() * 1000) & 0xFFFFFFFF
        idx = retry = 0

        while idx < pages:
            print(f"Progress: {idx/pages*100:5.1f}% ({idx+1}/{pages})", end='\r')

            msg = bytearray(272)
            struct.pack_into('<HHIHH', msg, 0, 0x0519, 268, ts, idx, pages)
            off = idx * 256
            msg[16:16+min(256, len(data)-off)] = data[off:off+256]
            self.send(msg)

            ok = False
            for _ in range(300):
                time.sleep(0.01)
                if self.ser.in_waiting:
                    self.buf.extend(self.ser.read(self.ser.in_waiting))
                
                if not (r := self.recv()):
                    continue
                if r[0] == 0x051A:
                    pg, err = struct.unpack_from('<HH', r[1], 4)
                    if pg != idx:
                        continue
                    if err:
                        print(f"\nError on page {idx}: {err}")
                        retry += 1
                        if retry > 3:
                            raise RuntimeError(f"Failed at page {idx}")
                        break
                    ok, retry = True, 0
                    break

            if ok:
                idx += 1
            else:
                print(f"\nTimeout on page {idx}")
                retry += 1
                if retry > 3:
                    raise RuntimeError(f"Failed at page {idx}")

        print(f"\nProgress: 100.0% ({pages}/{pages})")


def main():
    import argparse, sys
    
    p = argparse.ArgumentParser(description='UV-K5 firmware flasher')
    p.add_argument('firmware', help='Firmware file (.bin)')
    p.add_argument('-p', '--port', help='Serial port')
    p.add_argument('-l', '--list', action='store_true', help='List ports')
    args = p.parse_args()

    if args.list:
        for port in serial.tools.list_ports.comports():
            print(f"{port.device} - {port.description}")
        return

    port = args.port
    if not port:
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            sys.exit("No ports found")
        for i, pt in enumerate(ports, 1):
            print(f"{i}. {pt.device} - {pt.description}")
        port = input("Port: ").strip()
        if not port:
            sys.exit("Port not specified")

    try:
        with open(args.firmware, 'rb') as f:
            data = f.read()
        if len(data) > MAX_FW_SIZE:
            sys.exit(f"Firmware too large: {len(data)} > {MAX_FW_SIZE}")

        fl = Flasher(port)
        ver = fl.wait_dev()
        fl.handshake(ver)
        fl.flash(data)
        print("Success!")

    except Exception as e:
        sys.exit(f"Error: {e}")


if __name__ == '__main__':
    main()
