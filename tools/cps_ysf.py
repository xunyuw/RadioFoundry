#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Put a Fusion (YSF, kind 6) channel into a RadioFoundry codeplug image.

The channel is an ordinary open-firmware record (docs/spec/open-firmware-cps.md 7.3.3, 56 bytes, with the safe DMR
receive-only stand-in and the channel's frequencies) plus a keyed tag (docs/spec/channel-modes.md 2) carrying kind 6
and the YSF parameter block (docs/spec/ysf-codeplug.md 2.1). The zone table gains it, so the radio's zone list reaches
it.

  cps_ysf.py IMAGE.bin --name YSF --rx 439.500 --tx 439.500
  cps_ysf.py IMAGE.bin --list
"""
import argparse
import binascii
import struct
import sys

SETTINGS = 0xE000
SECTOR = 0x1000
KIND_YSF = 7        # channel-modes.md 4: 5 AES, 6 D-STAR, 7 Fusion

CHAN_BASE = 0x3790        # channel 1, 56 bytes each, channels 1..128
CHAN_STRIDE = 56
CHAN_USED = 0x3780        # 16-byte in-use bitmap
ZONE_BASE = 0x8030        # 7.3.4: zone slot 1
ZONE_STRIDE = 0xB0        # 80 channel numbers after the name
ZONE_COUNT = 68
ZONE_USED = 0x8010        # 9-byte in-use bitmap, bit j of byte i for slot i*8+j
CONTACT_BASE = 0x0F40     # not used here

FLAG4_RX_ONLY = 0x04
FLAG4_BW25 = 0x02


def kv_read(img):
    if len(img) < SETTINGS + 16:
        return [], False
    hdr = bytes(img[SETTINGS:SETTINGS + 16])
    if hdr[0:4] != b"RFKV" or hdr[4] != 1:
        return [], False
    hlen = hdr[5]
    dlen = struct.unpack_from("<H", hdr, 6)[0]
    crc = struct.unpack_from("<I", hdr, 8)[0]
    if hlen < 16 or hlen + dlen > SECTOR:
        return [], False
    data = bytes(img[SETTINGS + hlen:SETTINGS + hlen + dlen])
    if binascii.crc32(data) & 0xFFFFFFFF != crc:
        return [], False
    out, at = [], 0
    while at + 4 <= len(data):
        k, n = struct.unpack_from("<HH", data, at)
        at += 4
        if at + n > len(data):
            return [], False
        out.append((k, bytes(data[at:at + n])))
        at += n
    return out, True


def kv_write(img, entries):
    body = bytearray()
    for k, v in sorted(entries):
        body += struct.pack("<HH", k, len(v)) + v
    if len(body) > SECTOR - 16:
        raise SystemExit("the settings area is full")
    crc = binascii.crc32(bytes(body)) & 0xFFFFFFFF
    img[SETTINGS:SETTINGS + SECTOR] = b"\xFF" * SECTOR
    img[SETTINGS:SETTINGS + 16] = (b"RFKV" + bytes([1, 16]) + struct.pack("<H", len(body)) +
                                   struct.pack("<I", crc) + b"\0\0\0\0")
    img[SETTINGS + 16:SETTINGS + 16 + len(body)] = body


def name_check(name16):
    """The tag's name check (codeplug_tag.c codeplug_name_check): FNV-1a over the record's 16 name octets, 0xFF and
    everything past the first NUL read as 0, then folded to 16 bits."""
    h = 2166136261
    end = False
    for i in range(16):
        c = 0 if end else name16[i]
        if c == 0xFF:
            c = 0
        end = end or c == 0
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFF


def chan_name(img, n):
    at = CHAN_BASE + (n - 1) * CHAN_STRIDE
    raw = bytes(img[at:at + 16])
    if all(b in (0x00, 0xFF) for b in raw):
        return ""
    return raw.rstrip(b"\x00 ").decode("ascii", "replace")


def in_use(img, n):
    return (img[CHAN_USED + (n - 1) // 8] >> ((n - 1) % 8)) & 1


def bcd_freq(mhz):
    """438.500 MHz -> 43850000 tenths of Hz -> the LE word 0x43850000 (the spec's own example)."""
    tenths = int(round(mhz * 100000))
    bcd = 0
    for i in range(8):
        bcd |= ((tenths // (10 ** i)) % 10) << (4 * i)
    return bcd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--name", default="YSF")
    ap.add_argument("--dest", default="")
    ap.add_argument("--dgid", type=int, default=0)
    ap.add_argument("--rx", type=float, default=439.5)
    ap.add_argument("--tx", type=float, default=439.5)
    ap.add_argument("--zone", type=int, default=1)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    img = bytearray(open(args.image, "rb").read())
    entries, valid = kv_read(img)
    print("settings area: %s, %d entries" % ("valid" if valid else "empty/invalid", len(entries)))

    used = [n for n in range(1, 129) if in_use(img, n)]
    print("channels in use: %d" % len(used))
    for n in used[:12]:
        print("  %3d  %r" % (n, chan_name(img, n)))
    tags = [(k, v) for k, v in entries if 0x0400 <= k < 0x0400 + 1024]
    print("tags: %d" % len(tags))
    for k, v in tags[:8]:
        print("  channel %d: kind %d, %d bytes" % (k - 0x0400 + 1, v[0], len(v)))

    if args.list:
        for k, v in sorted(entries):
            print("  0x%04X %3d  %s" % (k, len(v), v.hex()))
        return 0

    num = max(used) + 1 if used else 1
    if num > 128:
        raise SystemExit("channel bank 0 is full")
    print("new channel: %d (%s), %g / %g MHz" % (num, args.name, args.rx, args.tx))

    # The base record: the safe stand-in (DMR, receive only) with the channel's name and frequencies
    at = CHAN_BASE + (num - 1) * CHAN_STRIDE
    rec = bytearray(b"\xFF" * CHAN_STRIDE)
    # 16 bytes of UTF-8, cut on a character boundary: a Chinese name is 3 bytes a character
    full = args.name.encode("utf-8")
    nb = full[:16]
    if len(nb) < len(full):
        # The cut fell inside a character: drop its continuation bytes, then the lead byte they belonged to
        while nb and (nb[-1] & 0xC0) == 0x80:
            nb = nb[:-1]
        if nb and nb[-1] >= 0xC0:
            nb = nb[:-1]
    rec[0:16] = nb.ljust(16, b"\x00")
    struct.pack_into("<I", rec, 0x10, bcd_freq(args.rx))
    struct.pack_into("<I", rec, 0x14, bcd_freq(args.tx))
    rec[0x18] = 1                                  # mode: DMR (the stand-in ysf-codeplug.md 2.1 names)
    rec[0x19] = 0                                  # power: the radio's own
    rec[0x1B] = 0                                  # TX time-out: the radio's
    struct.pack_into("<H", rec, 0x20, 0xFFFF)      # RX tone none
    struct.pack_into("<H", rec, 0x22, 0xFFFF)      # TX tone none
    rec[0x27:0x2A] = b"\x00\x16\x00"               # the vendor defaults these bytes replaced
    rec[0x2C] = 0                                  # color code
    rec[0x31] = 0                                  # timeslot 1
    rec[0x33] = FLAG4_RX_ONLY | FLAG4_BW25         # the stand-in is receive only, wide
    rec[0x34:0x36] = b"\x00\x00"
    rec[0x37] = 0                                  # squelch: the radio's default
    img[at:at + CHAN_STRIDE] = rec
    img[CHAN_USED + (num - 1) // 8] |= 1 << ((num - 1) % 8)

    # The tag: kind, no system, the name's check, flags, then the YSF block
    tag = bytearray([KIND_YSF, 0xFF]) + struct.pack("<H", name_check(bytes(rec[0:16]))) + bytes([0x01])
    tag += args.dest.encode("ascii", "replace")[:10].ljust(10, b" ")
    tag += bytes([args.dgid & 0x7F, args.dgid & 0x7F, 0x00])
    entries = [(k, v) for k, v in entries if k != 0x0400 + num - 1]
    entries.append((0x0400 + num - 1, bytes(tag)))
    kv_write(img, entries)
    print("tag written (%d bytes: kind %d)" % (len(tag), KIND_YSF))

    # The zone: append the channel to its list, so the zone list reaches it. A zone holds 80 numbers and its list
    # ends at the first 0
    zat = ZONE_BASE + (args.zone - 1) * ZONE_STRIDE
    if not ((img[ZONE_USED + (args.zone - 1) // 8] >> ((args.zone - 1) % 8)) & 1):
        raise SystemExit("zone %d is not in use: say --zone of one that is" % args.zone)
    zname = bytes(img[zat:zat + 16]).rstrip(b"\x00 ").decode("ascii", "replace")
    nums = struct.unpack_from("<80H", img, zat + 0x10)
    have = [x for x in nums if x]
    print("zone %d: %r, %d channels" % (args.zone, zname, len(have)))
    if num in have:
        print("  already holds channel %d" % num)
    elif len(have) >= 80:
        raise SystemExit("zone %d is full" % args.zone)
    else:
        struct.pack_into("<H", img, zat + 0x10 + len(have) * 2, num)
        print("  channel %d appended to it" % num)

    out = args.out or args.image.replace(".bin", "-ysf.bin")
    open(out, "wb").write(img)
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
