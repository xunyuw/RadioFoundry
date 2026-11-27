// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The CJK face (CONFIG_LANG_ZH): a one-bit glyph for every character core/ui/lang/zh.po uses, which tools/cjkfont.py
// rasterizes from an outline or pixel font into cjk_font_data.c. The Latin fonts have no glyph for Chinese, so text is
// UTF-8 and the text layer (gfx.c) looks each character up here.
//
// The cell is CONFIG_CJK_CELL pixels square. It is 16 by default, the height of the 8x16 font, so a line with Chinese
// keeps that height (docs/spec/ui.md 2.5). A layout that draws its words in the 6x12 font sets it to 12 and needs a
// font drawn at that size: scaling a 16 px outline down loses the strokes (tools/cjkfont.py).
#ifndef UI_CJK_FONT_H
#define UI_CJK_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define CJK_FONT_W		CONFIG_CJK_CELL
#define CJK_FONT_H		CONFIG_CJK_CELL
#define CJK_FONT_ROW_BYTES	((CJK_FONT_W + 7) / 8)
#define CJK_FONT_BYTES		(CJK_FONT_H * CJK_FONT_ROW_BYTES)

// The table carries one glyph per code point and the cell is square; the header's count must match what the tool wrote
#if CONFIG_CJK_GLYPHS && (CONFIG_CJK_CELL < 8 || CONFIG_CJK_CELL > 32)
#error "CONFIG_CJK_CELL must be 8..32"
#endif
// The glyphs are read from the data flash (CONFIG_CJK_FONT_IN_FLASH): the lookup fills one cell instead of returning a
// pointer into this image, so the two builds differ only in where the bitmap is
#if CONFIG_CJK_FONT_IN_FLASH
#define CJK_FONT_IN_FLASH	1
#endif

// The glyph of a code point: CJK_FONT_H rows of CJK_FONT_ROW_BYTES bytes, MSB the left of the cell, or NULL when the
// font has none (the text layer draws a box). Built only with CONFIG_CJK_GLYPHS; without it the call returns NULL.
// The pointer is valid until the next call when the glyphs come from the data flash
const uint8_t *cjk_font_glyph(uint32_t cp);

#if CONFIG_CJK_GLYPHS
// How many characters the table has, where its glyphs are (0 when they are in this image) and the index's FNV-1a. A
// tool compares the hash with the one the font it writes was built from, so a firmware and a flash from different runs
// are told apart (core/debuglink/debuglink_view.c reports them). Both shapes define all three
extern const uint16_t cjk_font_count;
extern const uint32_t cjk_font_addr;
extern const uint32_t cjk_font_index_hash;
#endif

#endif
