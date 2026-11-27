<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# A Chinese build, from a pack

A radio that shows Chinese needs three things written to it, in one order: the firmware image, the glyph bitmaps and
the pinyin tables. A *Chinese pack* holds all three and the tool that writes them, so there is nothing to build and
nothing to download separately.

A pack is made per radio (`rfoundry-<radio>-<version>-zh.zip`) and names the image it carries. If the name has
`-unofficial` in it, it was built from a modified tree and the RadioFoundry project does not support it: ask whoever
published the pack.

## Why three files

The firmware keeps the Chinese glyphs and the pinyin tables in the radio's **data flash**, not in the image. That
flash has room for 176 KB of glyphs; the 720 KB application region does not, once the rest of the firmware is in it.
So the image alone gives a radio that draws boxes where Chinese should be.

`rfoundry prepare` writes all three and checks them against each other first: the font carries a record of the
character count and the glyph index's hash, the firmware reports the index it looks characters up with, and a font
from another build is refused before anything is written. That check is the difference between correct glyphs and the
wrong character on every line.

## What you need

- the radio the pack names
- a USB cable that carries data (charge-only cables do not)
- Windows 10/11, Linux or macOS
- **not** the stock firmware file: a pack keeps the codec already in the radio

On Windows the radio's bootloader needs a driver the first time: see
[Installing](02-installing.md#windows-the-bootloader-driver).

## The order

1. **Unzip the pack** somewhere you can find it. Keep the folder structure: the paths below are relative to it.

2. **Put the radio in its bootloader.** Switch it off, then hold the two keys it uses for the bootloader while
   switching it on. Most TYT and Baofeng radios use the two side keys above PTT; the radio's own manual has the pair
   for its model. The screen stays dark and the radio appears as a USB device.

3. **Run one command** from the unzipped folder:

   ```
   Windows   rfoundry-windows-x86_64.exe prepare firmware\rfoundry-<radio>-<version>.bin ^
                 --font data\cjk.bin --pinyin data\pinyin.bin --board <radio>
   Linux     ./rfoundry-linux-x86_64 prepare firmware/rfoundry-<radio>-<version>.bin \
                 --font data/cjk.bin --pinyin data/pinyin.bin --board <radio>
   macOS     ./rfoundry-macos-* prepare firmware/rfoundry-<radio>-<version>.bin \
                 --font data/cjk.bin --pinyin data/pinyin.bin --board <radio>
   ```

   The pack's `README.txt` has these lines filled in with its own file names: copy them from there.

   The command checks the font against the radio's firmware, flashes the image, writes both data rows and reads each
   back. It prints `the radio draws Chinese` and exits 0 when it is done. Read its output rather than switching the
   radio off at the first pause.

4. **Switch the radio on.** It starts in Chinese. If it does not, open Settings > Display > Language (设置 > 显示 > 语言)
   and pick 中文.

5. **Type a Chinese name.** In a name field, each number key is the letters on it (2 = abc, 3 = def …). The bar above
   shows the readings of the digits typed so far; the arrow keys walk them and OK takes one.

## Characters the bitmaps do not carry

The pack's bitmaps cover every character GB2312 codes (7447 with the interface's own marks). A name that uses a
character outside that set — a rare ideograph, traditional characters, an emoji — draws a box. Nothing is broken; the
radio has no glyph for it.

To add one, render the table again with the characters you need and write it with the same command as step 3. The pack
carries the face the bitmaps came from in `font/`, and `tools/cjkfont.py` (from the project's repository) takes a list
of characters to include:

```
python3 tools/cjkfont.py --font font/<the face>.ttf --cell 12 --supersample 1 --gb2312 \
    --chars-file my-names.txt \
    --font-name "<the face and its version>" --copyright "<from the face>" \
    --license OFL-1.1 --spdx OFL-1.1 --out-c /tmp/font.c --out-bin my-font.bin
```

`my-names.txt` holds one name (or one character) per line. The command then needs the firmware rebuilt with the table
it wrote, which is why the project builds one image per font: changing what the table holds changes the index the
radio looks characters up with, and the `.json` beside `my-font.bin` is what rfoundry compares. In short: this is a
build, not a setting. Ask whoever published the pack to add the characters you need.

## When something goes wrong

| The command says | What it means |
|---|---|
| `the radio did not report its glyph table` | The radio is not running this pack's firmware. Step 3 flashes it; run the command again after that |
| `was built for N characters and … for M` | The font and the image are from different builds. Do not mix files from two packs |
| `no radio … in its bootloader` | The radio is not in its bootloader. Do step 2 and run it again |
| the display shows boxes | The image or the blobs did not all get written. Run step 3 again and read its output |
| DMR voice stops working | The radio's codec was not where the pack expected it. Flash again with your stock firmware file: `rfoundry flash --stock FILE …` ([Installing](02-installing.md#no-codec-no-codec-voice-off)) |

## Where the glyphs come from

The bitmaps are rendered from [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font) (SIL Open Font License
1.1), which allows redistributing the bitmaps it produced. The pack carries that license as `LICENSES/OFL-1.1.txt`
and does not carry the font file itself. The pinyin readings come from `pypinyin`. How the tables are built is in
[translating.md](../dev/translating.md).
