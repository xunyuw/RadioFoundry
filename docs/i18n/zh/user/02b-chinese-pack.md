<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# 从中文包安装

要让电台显示中文，需要按固定顺序写入三样东西：固件镜像、字形位图和拼音表。**中文包**（Chinese pack）把这三样和写入它们的工具装在一起，不用自己构建，也不用另外下载。

中文包按电台区分（`rfoundry-<电台>-<版本>-zh.zip`），并标明它带的是哪个镜像。如果文件名里有 `-unofficial`，说明它由修改过的代码树构建，RadioFoundry 项目不为其提供支持：请询问发布这个包的人。

## 为什么是三样

固件把中文字形和拼音表放在电台的**数据 flash** 里，不放在镜像里。那块 flash 放得下 176 KB 字形；而 720 KB 的应用区在装完其余固件后放不下。所以只刷镜像，电台会在本该显示中文的地方画方框。

`rfoundry prepare` 会写入这三样，并在写之前互相校验：字库里记着字符数和字形索引的哈希，固件会报告它查字用的索引，来自其他构建的字库会在写入之前被拒绝。这道校验决定了是字形正确，还是每一行都显示错误的字。

## 需要什么

- 中文包上写明的电台
- 一根能传数据的 USB 线（只能充电的线不行）
- Windows 10/11、Linux 或 macOS
- **不需要**原厂固件文件：中文包会保留电台里已有的 codec

Windows 上电台的 bootloader 第一次需要装驱动，见[安装](02-installing.md#windows-the-bootloader-driver)。

## 顺序

1. **解压中文包**到一个找得到的位置。保持目录结构：下面的路径都相对于它。

2. **让电台进入 bootloader。** 关机，然后在开机的同时按住该型号用于进入 bootloader 的两个键。多数 TYT 和宝锋电台用的是 PTT 上方那两个侧键；具体按键见电台自己的说明书。屏幕保持黑屏，电台会以 USB 设备的形式出现。

3. **在解压后的目录里运行一条命令**：

   ```
   Windows   rfoundry-windows-x86_64.exe prepare firmware\rfoundry-<电台>-<版本>.bin ^
                 --font data\cjk.bin --pinyin data\pinyin.bin --board <电台>
   Linux     ./rfoundry-linux-x86_64 prepare firmware/rfoundry-<电台>-<版本>.bin \
                 --font data/cjk.bin --pinyin data/pinyin.bin --board <电台>
   macOS     ./rfoundry-macos-* prepare firmware/rfoundry-<电台>-<版本>.bin \
                 --font data/cjk.bin --pinyin data/pinyin.bin --board <电台>
   ```

   中文包里的 `README.txt` 有你这一包对应的完整命令，直接从那里复制。

   这条命令会先校验字库与电台固件是否匹配，然后刷入镜像、写入两个数据行，并逐项读回校验。完成时打印 `the radio draws Chinese` 并以 0 退出。请读它的输出，不要在第一次停顿时就关掉电台。

4. **开机。** 电台会以中文启动。如果没有，进入 设置 > 显示 > 语言，选择「中文」。

5. **输入中文名字。** 在名字输入框里，每个数字键是对应的一组字母（2 = abc，3 = def……）。上方横条会显示已输入数字对应的读音，方向键切换，OK 选定。

## 字库里没有的字

包里的位图覆盖 GB2312 编码的全部字符（含界面自用的符号共 7447 个）。如果名字里用了这个范围以外的字——生僻字、繁体字、表情符号——会显示成方框。这不是故障，只是电台没有那个字的字形。

要加上它，需要用你需要的字符重新渲染字表，再用第 3 步同样的命令写入。包里 `font/` 目录放着位图所用的源字体，项目的 `tools/cjkfont.py` 可以带上你自己的字符清单：

```
python3 tools/cjkfont.py --font font/<源字体>.ttf --cell 12 --supersample 1 --gb2312 \
    --chars-file my-names.txt \
    --font-name "<源字体及版本>" --copyright "<来自源字体>" \
    --license OFL-1.1 --spdx OFL-1.1 --out-c /tmp/font.c --out-bin my-font.bin
```

`my-names.txt` 每行一个名字或一个字符。之后还需要用新字表重新构建固件——这正是项目为每种字表单独出一个镜像的原因：字表内容一变，电台查字的索引就变，而 `my-font.bin` 旁的 `.json` 就是 rfoundry 用来比对的依据。总之：这是一次构建，不是一个设置项。请让发布这个包的人加上你需要的字。

## 出问题时

| 命令提示 | 含义 |
|---|---|
| `the radio did not report its glyph table` | 电台运行的不是这个包里的固件。第 3 步会刷入它；刷完再运行一次 |
| `was built for N characters and … for M` | 字库和镜像是不同构建的。不要把两个包里的文件混用 |
| `no radio … in its bootloader` | 电台没在 bootloader 里。做第 2 步再运行 |
| 屏幕显示方框 | 镜像或数据没有全部写入。重做第 3 步并读输出 |
| DMR 语音失效 | 电台的 codec 不在这个包预期的位置。用原厂固件文件重新刷：`rfoundry flash --stock FILE …`（见[安装](02-installing.md#no-codec-no-codec-voice-off)） |

## 字形从哪来

位图由 [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font)（SIL Open Font License 1.1）渲染而成，该许可允许再分发它生成的位图。中文包以 `LICENSES/OFL-1.1.txt` 附带该许可证，但不附带字体文件本身。拼音读音来自 `pypinyin`。这些表如何生成，见 [translating.md](../dev/translating.md)。
