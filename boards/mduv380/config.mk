# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# MD-UV380 feature defaults (config/features.mk lists them all). 720 KB for code and read-only data: flash sectors 3-9;
# the codec sectors 10 and 11 are never linked into (boards/mduv380/memory.ld, docs/spec/codec-placement.md).
# 160x128 display, every row visible. The fault trace, the large font and LTO follow the platform
# (platform/stm32f4/config.mk)
# Safe area: the TYT housing's edge crowds the top row and the two left columns; unconfirmed on this radio
# (hardware-mduv380.md 4)
CONFIG_DISPLAY_INSET ?= 0
CONFIG_DISPLAY_INSET_TOP ?= 1
CONFIG_DISPLAY_INSET_LEFT ?= 2
# APRS receive takes the HR_C6000's FM receive audio from the I2S stream (docs/spec/aprs.md 5.3). The iGate (RF to
# APRS-IS through tools/aprs_gate.py, 1.9 KB) is off by default
CONFIG_APRS_IGATE ?= n
# The maximal configuration builds the hotspot rather than Voice FR, the two wanting the same RAM (below): a CONFIG_VARS
# entry because VW is on by default, and MAXIMAL_SKIP alone stops only a switch the maximal build would turn on
MAXIMAL_VARS := LTO=1 CONFIG_YSF_VW=n
# Left out of tools/ci.sh's maximal configuration: PPP stands in for the USB network adapter on UART-only radios
# (docs/spec/ppp.md), and this one has the adapter; the APRS iGate is off by default (above); French, off by default,
# so that the maximal build keeps satellite tracking and still fits the app region; Chinese too, whose code point index
# and pinyin search fill the app region beside every other switch (36 KB over). The `chinese` extra builds it instead,
# without satellite tracking (board.toml). Only its bitmaps live in the data flash (below), not its index or text.
# Voice FR is left out by MAXIMAL_VARS above, so the maximal build has the hotspot
MAXIMAL_SKIP := NET_PPP APRS_IGATE LANG_FR LANG_ZH LANG_DEFAULT_ZH

# What the radio's hardware can't do, one switch per line with the reason (tools/boards.py checks there is one;
# docs/build-config.md, "Unsupported, not written, unconfirmed"): ci's maximal configuration leaves these off
BOARD_UNSUPPORTED += FONT_XL	# the 160x128 panel is too small for the large font (it is for 240 px and wider)
BOARD_UNSUPPORTED += FONT_BIG	# the 9x18 font is drawn only on panels 220 px and wider; this one is 160x128
CONFIG_AM_RX ?= n
BOARD_UNSUPPORTED += AM_RX	# no air band receiver: the AT1846S is FM only (docs/spec/airband-and-broadcast.md)
CONFIG_BCAST_FM ?= n
BOARD_UNSUPPORTED += BCAST_FM	# no broadcast receiver (docs/spec/airband-and-broadcast.md)
BOARD_UNSUPPORTED += NAND_STORAGE	# the data flash is a 16 MB SPI NOR (docs/spec/hardware-mduv380.md, the overview)
BOARD_UNSUPPORTED += VIBRATE	# no vibration motor (docs/user/23-radios.md, What the MD-UV380 lacks)
BOARD_UNSUPPORTED += FAULT_TRACE	# needs a host UART: the host port is USB, and the one UART is the G models' GPS line

# Chinese on this radio: a 12 px cell (CONFIG_CJK_CELL), the size of the 6x12 font this 160 px class already draws its
# words in, so the layout stays as it is in English instead of moving up to the 8x16 one. With the full GB2312 face
# (tools/cjkfont.py --gb2312) any name a user types draws, and with CONFIG_PINYIN a Chinese name is entered on the
# radio. Those two tables do not fit the app region (176 KB of glyphs and 32 KB of pinyin tables), so that build keeps
# them in the data flash at 0xF40000 and 0xF6C000 (below) and `rfoundry font` writes them there. Off by default like
# Chinese on every radio: the `chinese` CI extra builds it, and a radio without the blob burned in draws boxes
CONFIG_CJK_CELL ?= 12
BOARD_NOT_WRITTEN += DSTAR	# GMSK, and this board has no flat FM transmit tap (docs/spec/dstar-modem.md 1.1)
# Voice FR is not a flat-tap feature: it is a codec (core/mbe) on the same C4FM route DN takes, which this board runs.
# It is on by default here. What it costs is RAM: 4,288 B measured, and this radio has room for exactly one of VW and the
# hotspot, so the hotspot is the choice instead (below). A platform whose vocoder lends the codec's state block
# (HAL_VOCODER_LEND_BYTES) spends a few hundred bytes instead (296 B where the vocoder lends); this one lends nothing, so
# radio_ysf_vw keeps its 3,964 B here (radio_ysf.h RADIO_YSF_VW_IN_AREA). 130,960 B of 131,068 with VW on and the hotspot
# off: 108 B free, which is why nothing else may be added to this build (measured 27 Nov 2026)
CONFIG_YSF_VW ?= $(CONFIG_YSF)

# The hotspot and Voice FR take the same RAM, and this board has them for one: its buffers are 2,496 B, VW's codec state
# 4,288 B, and the build has 1,900 B left with neither. A handheld is more often on the air than acting as a modem, so
# VW is the default and the hotspot is the choice: CONFIG_HOTSPOT=y CONFIG_YSF_VW=n builds the hotspot and leaves VW's
# codec out. CI builds the default (VW on), the maximal configuration with the hotspot off (MAXIMAL_SKIP), and the
# `chinese` extras
CONFIG_HOTSPOT ?= n
BOARD_DEFAULT_OFF += HOTSPOT	# works, and needs the 2,496 B of RAM this build gives to Voice FR

# Fusion is on by default: this board reaches C4FM through its HR_C6000's FM copy, not the flat FM tap Fusion's first
# route needs (docs/spec/ysf-modem.md 1.7), so CONFIG_YSF_AIR carries it. Checked both ways on air against a stock radio
# (docs/testing/ysf-bench.md). It costs 2.5 KB of RAM this radio does not have to spare, and the split duplex repeater
# the hotspot can run is what goes: that needs two radios and is the one hotspot feature this board loses. 705,264 B of
# the 720 KB region and 129,168 B of RAM (measured 27 Nov 2026)
CONFIG_YSF ?= y
CONFIG_YSF_AIR ?= $(CONFIG_YSF)
CONFIG_HOTSPOT_SPLIT ?= n
