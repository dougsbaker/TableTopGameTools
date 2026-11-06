"""Reset the board, collect @@SHOT screenshots from its serial output, and write PNGs."""
import base64
import binascii
import struct
import sys
import time
import zlib
from pathlib import Path

import serial

port, out_dir, timeout_s = sys.argv[1], Path(sys.argv[2]), float(sys.argv[3])
out_dir.mkdir(parents=True, exist_ok=True)


def open_port(t):
    deadline = time.time() + t
    while True:
        try:
            return serial.Serial(port, 115200, timeout=0.2)
        except serial.SerialException:
            if time.time() > deadline:
                raise
            time.sleep(0.1)


ser = open_port(5)
ser.dtr = False
ser.rts = True
time.sleep(0.1)
ser.rts = False
ser.close()
time.sleep(0.3)
ser = open_port(8)

raw = b""
end = time.time() + timeout_s
while time.time() < end:
    try:
        raw += ser.read(65536)
    except serial.SerialException:
        ser.close()
        ser = open_port(5)
    if b"@@DONE" in raw:
        break
ser.close()
(out_dir / "capture.log").write_bytes(raw)


def unpack(data, count):
    px = []
    i = 0
    while len(px) < count:
        n = data[i]
        i += 1
        if n >= 128:
            v = data[i] | data[i + 1] << 8
            i += 2
            px.extend([v] * (n - 125))
        else:
            for _ in range(n + 1):
                px.append(data[i] | data[i + 1] << 8)
                i += 2
    return px


def write_png(path, w, h, px):
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for v in px[y * w:(y + 1) * w]:
            r = (v >> 11) & 31
            g = (v >> 5) & 63
            b = v & 31
            rows += bytes(((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


name = None
buf = bytearray()
good = set()
for line in raw.decode("ascii", errors="replace").splitlines():
    if line.startswith("@@SHOT "):
        _, name, w, h = line.split()
        w, h = int(w), int(h)
        buf = bytearray()
    elif line.startswith("@@D ") and name:
        buf += base64.b64decode(line[4:].strip())
    elif line.startswith("@@END ") and name:
        _, _, n, crc = line.split()
        if name in good:
            name = None
            continue
        if len(buf) != int(n):
            print(f"{name}: got {len(buf)} of {n} bytes")
            name = None
            continue
        px = unpack(buf, w * h)
        got = binascii.crc32(struct.pack(f"<{w*h}H", *px)) & 0xFFFFFFFF
        status = "ok" if got == int(crc, 16) else f"CRC MISMATCH {got:08x} != {crc}"
        if status == "ok":
            good.add(name)
        write_png(out_dir / f"{name}.png", w, h, px)
        print(f"{name}: {n} bytes packed, {status}")
        name = None
print("done" if b"@@DONE" in raw else "timed out before @@DONE")
