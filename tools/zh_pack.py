#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Pack a Chinese build for people who do not build firmware (docs/user/02b-chinese-pack.md).

A Chinese image keeps its glyph bitmaps and pinyin tables in the radio's data flash, so a user needs three things and
has to do them in one order: the firmware image, the two blobs, and the tool that writes all three. This writes one
zip holding them, a README that says the order, and a checksum file:

  tools/zh_pack.py --board <radio> --version v26.11 --out dist \
      --font build/cjk.bin --pinyin build/pinyin.bin --face fusion-pixel-12px-proportional-zh_hans.ms.bitmap.ttf

--face adds the font the bitmaps were rasterized from. Nothing in the flashing path needs it: the bitmaps are already
rendered, so the radio and rfoundry work from them alone. It is there so a user can re-render the table with their own
character list (tools/cjkfont.py --font ... --chars-file ...) without hunting for a face of the same version, which is
what draws the marks and names GB2312 does not code. The face is SIL Open Font License 1.1: the pack carries OFL-1.1
and the face's own notice, which is what that license asks of anyone redistributing it.

The images come from tools/release.py's output (dist/rfoundry-<board>[-<option>]-<version>.json); the blobs are what
tools/cjkfont.py --out-bin and tools/pinyin_table.py --out-bin wrote. Both blobs must carry their .json: it is what
rfoundry matches against the firmware's own table before anything is written, and a pack without it is one whose
glyphs can be the wrong build's.

The pack carries the blobs, not the font face they came from: the bitmaps are OFL-1.1 (LICENSES/OFL-1.1.txt is in the
zip), the face itself is not redistributed (docs/dev/translating.md).
"""
import argparse
import glob
import hashlib
import importlib.util
import json
import os
import shutil
import sys
import zipfile


def _release_version():
    """tools/release_version.py: the release names, so a fork's build is never named like one of ours."""
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location("release_version", os.path.join(here, "release_version.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


release_version = _release_version()


def is_fork(tag):
    return release_version.is_fork(tag)

# What a Chinese image needs on the radio, and what a user must be told, in the order they do it. The page name is
# what the zip's README points at on the site; the site's own copy of this text is docs/user/02b-chinese-pack.md.
README = """\
RadioFoundry {version} - {board_name}, with Chinese

This pack has everything a {board_name} needs to show and type Chinese, and nothing else. Read this to the end before
you start: the order below matters.

What is in this folder
----------------------
{firmware_list}
  data/{font_name}        the glyph bitmaps, {font_bytes} bytes, every character GB2312 codes
  data/{font_name}.json   which glyph table the bitmaps are (rfoundry compares it with the firmware)
  data/{pinyin_name}      the pinyin tables, {pinyin_bytes} bytes, for typing Chinese on the keypad
{tool_list}  SHA256SUMS              every file above, for checking a download that went wrong
{face_list}
Nothing above but SHA256SUMS is optional: every file it lists goes on the radio in the order below.

Why three files
---------------
The firmware keeps the Chinese glyphs and the pinyin tables in the radio's data flash, not in the image: that flash
has room for them and the 720 KB image does not. So the image alone gives you a radio that draws boxes where Chinese
should be. rfoundry writes the image and both blobs, and checks that the glyphs are the ones this firmware looks
characters up with - a font from another build draws the wrong character for every one of them.

What you need
-------------
  - the radio, {board_name}
  - a USB cable that carries data (some charge-only cables do not)
  - Windows 10/11, Linux or macOS. On Windows, the radio's bootloader needs a driver the first time: see
    https://radiofoundry.org/docs/user/02-installing.html#windows-driver
  - the radio's own stock firmware file is NOT needed: this pack keeps the codec already in the radio

The order (do not skip a step)
------------------------------
1. Unzip this pack somewhere you can find it. Keep the folder structure.

2. Put the radio in its bootloader: switch it off, then hold the two keys the {board_name} uses for it while
   switching it on. (Its manual has them; on most TYT/Baofeng radios it is the two side keys above PTT.) The
   screen stays dark and the radio appears as a USB device.

3. Open a terminal in this folder and run, in this order:

     Windows:   .\\rfoundry-windows-x86_64.exe prepare firmware\\{image} \\
                    --font data\\{font_name} --pinyin data\\{pinyin_name} --board {board_id}
     Linux:     ./rfoundry-linux-x86_64 prepare firmware/{image} \\
                    --font data/{font_name} --pinyin data/{pinyin_name} --board {board_id}
     macOS:     ./rfoundry-macos-* prepare firmware/{image} \\
                    --font data/{font_name} --pinyin data/{pinyin_name} --board {board_id}

   One command does all of it: it reads the radio's firmware table and refuses a font that is not this build's,
   flashes the image, writes the two blobs and reads each back. It prints "the radio draws Chinese" when it is done
   and exits non-zero when anything is wrong - read what it says rather than switching the radio off.

   If the radio is in its bootloader, rfoundry finds it. If it is not, it says so: do step 2 and run it again.

4. Switch the radio on. It starts in Chinese. If it does not, go to Settings > Display > Language (设置 > 显示 > 语言)
   and pick 中文.

5. Type a Chinese name: in the channel editor, hold the key that opens the text editor and use the number keys - each
   key is the letters on it (2 = abc, 3 = def ...), and the bar above shows the readings to pick from.

If something goes wrong
-----------------------
  - "the radio did not report its glyph table": the radio is not running the firmware this pack carries. Flash it
    first (step 3 does that), then run the command again.
  - "was built for N characters and ... for M": the font and the image are from different builds. Do not mix packs;
    use the whole folder from one zip.
  - The display shows boxes: the firmware in the radio is not this one, or the blobs were not written. Run step 3
    again and read its output.
  - DMR voice stops working after flashing: the radio's codec was not where this pack expected. Flash again with
    your stock firmware file: `rfoundry flash --stock FILE ...` (the project's install guide has this in full).

This is {version}{unofficial}
"""

UNOFFICIAL = (", an unofficial build of RadioFoundry, not supported by the RadioFoundry project: ask whoever\n"
              "published this pack. The project's own releases are at https://github.com/RadioFoundry/RadioFoundry.")

# The same walkthrough in Chinese, as its own file: a reader who needs the Chinese build reads Chinese first, and the
# English README's job is then only the file list. Kept in step with README above.
README_ZH = """\
RadioFoundry {version} - {board_name} 中文固件包

这个包里有让 {board_name} 显示和输入中文所需的全部内容。开始前请先看完这份说明：下面的顺序很重要。

包内文件
--------
{firmware_list}
  data/cjk.bin        字形位图，{font_bytes} 字节，GB2312 编码的全部汉字
  data/cjk.bin.json   这份位图对应哪套字表（rfoundry 会拿它和固件比对）
  data/pinyin.bin     拼音表，{pinyin_bytes} 字节，用于用键盘输入中文
{tool_list}
  SHA256SUMS          以上所有文件的校验和，用于检查下载是否完整
{face_list}
为什么是三样
------------
固件把中文字形和拼音表放在电台的数据 flash 里，不放在镜像里：那块 flash 放得下 180 KB 字形，而 720 KB 的应用区
在装完其余固件后放不下。所以只刷镜像的话，电台会在本该显示中文的地方画方框。

rfoundry 会写入这三样，并在写之前互相校验：字库里记着字符数和字形索引的哈希，固件会报告它查字用的索引。来自其他
构建的字库会在写入之前被拒绝。

需要什么
--------
  - 电台，{board_name}
  - 一根能传数据的 USB 线（只能充电的线不行）
  - Windows 10/11、Linux 或 macOS
  - 不需要原厂固件文件：本包会保留电台里已有的 codec

Windows 上第一次使用需要给 bootloader 装驱动，见项目网站「安装」页。

操作顺序（不要跳过任何一步）
--------------------------
1. 把本包解压到一个找得到的位置，保持目录结构不变。

2. 让电台进入 bootloader：先关机，然后在开机的同时按住 {board_name} 用于进入 bootloader 的两个键（多数 TYT /
   宝锋电台是 PTT 上方那两个侧键，具体见电台说明书）。屏幕保持黑屏，电脑上会出现一个 USB 设备。

3. 在本文件夹里打开终端，按下面的顺序执行（本包里已带工具，不用另外下载）：

     Windows:   rfoundry-v26.11-bi4aoq-windows-x86_64.exe prepare firmware\\{image} ^
                    --font data\\cjk.bin --pinyin data\\pinyin.bin --board {board_id}
     Linux:     ./rfoundry-v26.11-bi4aoq-linux-x86_64 prepare firmware/{image} \\
                    --font data/cjk.bin --pinyin data/pinyin.bin --board {board_id}
     macOS:     ./rfoundry-v26.11-bi4aoq-macos-x86_64 prepare firmware/{image} \\
                    --font data/cjk.bin --pinyin data/pinyin.bin --board {board_id}

   一条命令做完全部：先校验字库与固件是否匹配（不匹配会拒绝，一个字都不会写），再刷入镜像、写入两个数据行、逐项
   读回校验。成功时最后一行是 the radio draws Chinese。请读它的输出，不要在第一次停顿时就关掉电台。

4. 开机。电台会以中文启动。如果没有，进入 设置 > 显示 > 语言，选择「中文」。

5. 输入中文名字：在名字输入框里，每个数字键是对应的一组字母（2 = abc，3 = def……），上方横条显示已输入数字对应
   的读音，方向键切换，OK 选定。

出问题时
--------
  - the radio did not report its glyph table：电台运行的不是本包的固件。第 3 步会刷入它，刷完再执行一次。
  - was built for N characters and ... for M：字库和镜像是不同构建的。不要混用两个包里的文件，用同一个 zip。
  - 屏幕显示方框：固件不是这个，或数据没写进去。重做第 3 步并读输出。
  - DMR 语音失效：电台的 codec 不在本包预期的位置。用原厂固件文件重新刷：
    rfoundry flash --stock 原厂固件文件 ...（项目网站「安装」页有完整说明）。

这是 {version}{unofficial}
"""


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_images(dist, board_id, version):
    """[(manifest dict, image path, manifest path)] for a board's images in a tools/release.py output directory."""
    out = []
    for p in sorted(glob.glob(os.path.join(dist, "rfoundry-%s*-%s.json" % (board_id, version)))):
        with open(p, encoding="utf-8") as f:
            m = json.load(f)
        if m.get("board") != board_id:
            continue
        img = os.path.join(os.path.dirname(p), m.get("image", {}).get("file", ""))
        if not os.path.isfile(img):
            print("zh_pack: %s names %s, which is not there" % (p, img), file=sys.stderr)
            continue
        out.append((m, img, p))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description="Pack a Chinese build for people who do not build firmware")
    ap.add_argument("--board", required=True, help="the radio (its id, as boards/<id>)")
    ap.add_argument("--version", required=True, help="the release name the images were built with (v26.11-bi4aoq)")
    ap.add_argument("--dist", required=True, help="the tools/release.py output directory holding the images")
    ap.add_argument("--font", required=True, help="the glyph bitmaps (tools/cjkfont.py --out-bin); its .json beside it")
    ap.add_argument("--pinyin", required=True, help="the pinyin tables (tools/pinyin_table.py --out-bin)")
    ap.add_argument("--tools", help="a directory of rfoundry executables to include (rfoundry-*-<os>-<arch>[*])")
    ap.add_argument("--face", help="the font the bitmaps were rasterized from (OFL-1.1), so the tables can be "
                                   "re-rendered with another character list; optional, nothing needs it to flash")
    ap.add_argument("--out", default="dist", help="where to write the zip (default: dist)")
    args = ap.parse_args(argv)

    images = find_images(args.dist, args.board, args.version)
    if not images:
        print("zh_pack: no image for %s at %s in %s (tools/release.py --board %s --version %s)"
              % (args.board, args.version, args.dist, args.board, args.version), file=sys.stderr)
        return 2

    font_meta = args.font + ".json"
    if not os.path.isfile(font_meta):
        print("zh_pack: %s has no .json: without it rfoundry cannot tell whether the glyphs match the firmware, "
              "which is the one check that keeps a wrong character off every one of them" % args.font, file=sys.stderr)
        return 2
    with open(font_meta, encoding="utf-8") as f:
        fm = json.load(f)

    tools = []
    if args.tools:
        tools = sorted(glob.glob(os.path.join(args.tools, "rfoundry-*")))
        tools = [t for t in tools if os.path.isfile(t) and not t.endswith((".json", ".txt", ".zip"))]
    if not tools:
        print("zh_pack: warning: no rfoundry executables in --tools, so the zip carries no tool to write the pack",
              file=sys.stderr)

    os.makedirs(args.out, exist_ok=True)
    # A fork's name (v26.11-bi4aoq) says so in the file name too: a zip whose name reads like the project's own
    # release is one a user takes for ours
    unofficial_name = "-unofficial" if is_fork(args.version) else ""
    name = "rfoundry-%s-%s-zh%s" % (args.board, args.version, unofficial_name)
    zip_path = os.path.join(args.out, name + ".zip")

    board_name = images[0][0].get("board_name") or args.board
    unofficial = UNOFFICIAL if is_fork(args.version) else ""
    # The default image is the one whose file name has no build-option part (rfoundry-<board>-<version>.bin against
    # rfoundry-<board>-10w-<version>.bin): it is the one a user of the plain radio takes, and the others are said to be
    # the 10 W or the other band rather than left to guess
    plain = "rfoundry-%s-%s.bin" % (args.board, args.version)
    default = next((x for x in images if os.path.basename(x[1]) == plain), images[0])
    image = os.path.basename(default[1])
    lines, lines_zh = [], []
    for m, img, man in images:
        tag = "" if (m, img, man) == default else " (%s build)" % os.path.basename(img).split("-%s" % args.version)[0] \
            .replace("rfoundry-%s-" % args.board, "").upper()
        tag_zh = "" if (m, img, man) == default else "（%s 版本）" % os.path.basename(img).split("-%s" % args.version)[0] \
            .replace("rfoundry-%s-" % args.board, "").upper()
        lines.append("  firmware/%s%s" % (os.path.basename(img), tag))
        lines_zh.append("  firmware/%s%s" % (os.path.basename(img), tag_zh))
        if (m, img, man) == default:
            lines.append("  firmware/%s     its manifest, which rfoundry checks the image against"
                         % os.path.basename(man))
            lines_zh.append("  firmware/%s     它的清单，rfoundry 用它校验镜像" % os.path.basename(man))
    firmware_list = "\n".join(lines)
    firmware_list_zh = "\n".join(lines_zh)
    if tools:
        tool_list = "\n".join("  %s" % os.path.basename(t) for t in tools) + "\n"
    else:
        tool_list = ("  (no rfoundry executable in this pack: use the one from the release page)\n")
    if args.face:
        face_list = ("  font/%s\n      the face the glyphs were rendered from (OFL-1.1), for rendering the table\n"
                     "      again with characters of your own; nothing needs it to flash\n" % os.path.basename(args.face))
    else:
        face_list = ""
    readme = README.format(
        version=args.version, board_name=board_name, board_id=args.board,
        image=image, manifest=os.path.basename(default[2]),
        font_name=os.path.basename(args.font), pinyin_name=os.path.basename(args.pinyin),
        font_bytes=fm.get("bytes", os.path.getsize(args.font)),
        pinyin_bytes=os.path.getsize(args.pinyin),
        firmware_list=firmware_list, tool_list=tool_list, face_list=face_list,
        unofficial=unofficial)
    # the Chinese copy of the same thing, its file list in the same words
    zh_face = ""
    if args.face:
        zh_face = ("  font/%s\n      位图所用的源字体（OFL-1.1），用于按你需要的字重新渲染字表；刷机不需要它\n"
                   % os.path.basename(args.face))
    readme_zh = README_ZH.format(
        version=args.version, board_name=board_name, board_id=args.board,
        image=image, manifest=os.path.basename(default[2]),
        font_name=os.path.basename(args.font), pinyin_name=os.path.basename(args.pinyin),
        font_bytes=fm.get("bytes", os.path.getsize(args.font)),
        pinyin_bytes=os.path.getsize(args.pinyin),
        firmware_list=firmware_list_zh, tool_list=tool_list, face_list=zh_face,
        unofficial=(", 一个非官方构建，RadioFoundry 项目不为其提供支持：请询问发布这个包的人。项目的官方版本见\n"
                    "https://github.com/RadioFoundry/RadioFoundry。" if unofficial else ""))

    written = []
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        def add(arc, path):
            z.write(path, arc)
            written.append((arc, path))

        # README first: it is what a user reads before doing anything, and the order it gives is the whole point
        z.writestr("README.txt", readme)
        # the same walkthrough in Chinese, as its own file a reader opens first
        z.writestr("安装说明.txt", readme_zh)
        for m, img, man in images:
            add("firmware/" + os.path.basename(img), img)
            add("firmware/" + os.path.basename(man), man)
        add("data/" + os.path.basename(args.font), args.font)
        add("data/" + os.path.basename(font_meta), font_meta)
        add("data/" + os.path.basename(args.pinyin), args.pinyin)
        # The face is optional and nothing in the flashing path reads it; it is here so the tables can be re-rendered
        # with a character list of the user's own, which is the only reason to want it
        if args.face:
            add("font/" + os.path.basename(args.face), args.face)
        for t in tools:
            add(os.path.basename(t), t)
        # the OFL the glyph bitmaps are under: the face is not redistributed, the bitmaps it made are
        lic = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "LICENSES", "OFL-1.1.txt")
        if os.path.isfile(lic):
            add("LICENSES/OFL-1.1.txt", lic)
        sums = "".join("%s  %s\n" % (sha256(p), a) for a, p in written)
        z.writestr("SHA256SUMS", sums)

    size = os.path.getsize(zip_path)
    print("zh_pack: %s (%d files, %d bytes)" % (zip_path, len(written) + 1, size))
    for m, img, _man in images:
        print("  image  %s (%d bytes)" % (os.path.basename(img), os.path.getsize(img)))
    print("  font   %s (%d bytes, %s glyphs)" % (os.path.basename(args.font), os.path.getsize(args.font),
                                                 fm.get("glyphs")))
    print("  pinyin %s (%d bytes)" % (os.path.basename(args.pinyin), os.path.getsize(args.pinyin)))
    if tools:
        print("  tools  %s" % ", ".join(os.path.basename(t) for t in tools))
    return 0


if __name__ == "__main__":
    sys.exit(main())
