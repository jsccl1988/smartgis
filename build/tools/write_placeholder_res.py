# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Write tiny ICO/BMP stand-ins for missing 2010 .rc binary resources.
# Not product art — only so rc.exe can run once MFC headers exist.

import os
import re
import sys


def write_ico(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    w = h = 16
    xor = bytes([0x2E, 0x7D, 0x32, 0xFF]) * (w * h)
    andb = bytes(8 * h)
    dibh = 40
    img = dibh + len(xor) + len(andb)
    ico = bytearray()
    ico += bytes([0, 0, 1, 0, 1, 0, w, h, 0, 0, 1, 0, 32, 0])
    ico += img.to_bytes(4, "little") + (22).to_bytes(4, "little")
    ico += dibh.to_bytes(4, "little") + w.to_bytes(4, "little") + (h * 2).to_bytes(4, "little")
    ico += (1).to_bytes(2, "little") + (32).to_bytes(2, "little") + (0).to_bytes(4, "little")
    ico += (len(xor) + len(andb)).to_bytes(4, "little") + (0).to_bytes(4, "little") * 4
    ico += xor + andb
    with open(path, "wb") as f:
        f.write(ico)


def write_bmp(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pixels = bytes(
        [0x2E, 0x7D, 0x32, 0x2E, 0x7D, 0x32, 0, 0, 0x2E, 0x7D, 0x32, 0x2E, 0x7D, 0x32, 0, 0]
    )
    b = bytearray(b"BM")
    b += (54 + len(pixels)).to_bytes(4, "little") + (0).to_bytes(4, "little") + (54).to_bytes(4, "little")
    b += (40).to_bytes(4, "little") + (2).to_bytes(4, "little") + (2).to_bytes(4, "little")
    b += (1).to_bytes(2, "little") + (24).to_bytes(2, "little") + (0).to_bytes(4, "little")
    b += len(pixels).to_bytes(4, "little") + (0).to_bytes(4, "little") * 4
    b += pixels
    with open(path, "wb") as f:
        f.write(b)


def decode_rc(raw):
    for enc in ("utf-8", "gbk", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    created = 0
    for dirpath, _dirs, files in os.walk(os.path.join(root, "src")):
        for name in files:
            if not name.lower().endswith(".rc"):
                continue
            rc = os.path.join(dirpath, name)
            text = decode_rc(open(rc, "rb").read())
            for m in re.finditer(r'res\\+([^"\r\n]+)', text):
                rel = m.group(1).strip()
                if rel.lower().endswith(".rc2"):
                    continue
                dest = os.path.join(dirpath, "res", rel)
                if os.path.exists(dest):
                    continue
                if rel.lower().endswith(".bmp"):
                    write_bmp(dest)
                elif rel.lower().endswith(".ico"):
                    write_ico(dest)
                else:
                    continue
                created += 1
                print(dest)
    print("created", created)


if __name__ == "__main__":
    main()
