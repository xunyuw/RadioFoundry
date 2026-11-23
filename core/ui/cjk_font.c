// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Lookup in the generated CJK face: a sorted array of the code points it has, and a glyph per entry, so a character
// costs a binary search and no RAM. cjk_font_data.c is built by tools/cjkfont.py from the catalogs themselves; this
// file is the search, and stays small on purpose.
//
// Two shapes, chosen by what the generated table holds (tools/cjkfont.py):
//  - cjk_font_bits: the glyphs are in this image, right after the code points, and the lookup returns a pointer into it.
//    What a build of the interface's own characters uses: 800 glyphs is 25 KB of flash.
//  - cjk_font_addr: the glyphs are in the data flash at that address (a whole GB2312 vocabulary is 90 KB, which the
//    application region has no room for). The lookup copies the one glyph into a static cell and returns that: the text
//    layer draws it at once, before the next character (core/ui/gfx.c), so one cell serves every caller and costs no RAM
//    per call. A read from the data flash wants a lock and an SPI transaction, so a glyph costs some microseconds more
//    than a pointer; a screen of forty Chinese characters still draws in a few milliseconds.
#include <stddef.h>
#include "cjk_font.h"
#include "board.h"
#include "storage/storage.h"

#ifdef CJK_FONT_IN_FLASH
// One glyph from the data flash, with its bus shared as every other reader takes it (core/satellite/store.c)
static bool font_read(uint32_t addr, void *buf, uint32_t len)
{
	if (board.flash == NULL || board.flash->read == NULL)
		return false;
	storage_lock();
	bool ok = board.flash->read(addr, buf, len);
	storage_unlock();
	return ok;
}
#endif

const uint8_t *cjk_font_glyph(uint32_t cp)
{
#if CONFIG_CJK_GLYPHS
	// core/ui/cjk_font_data.c, built by tools/cjkfont.py
	extern const uint16_t cjk_font_cp[];
	extern const uint16_t cjk_font_count;
#ifdef CJK_FONT_IN_FLASH
	extern const uint32_t cjk_font_addr;
#else
	extern const uint8_t cjk_font_bits[];
#endif
	if (cp > 0xFFFF)
		return NULL;			// the table holds the BMP only: every CJK block is below it
	uint16_t lo = 0, hi = cjk_font_count;
	while (lo < hi) {
		uint16_t mid = (uint16_t)((lo + hi) / 2u);
		uint16_t at = cjk_font_cp[mid];
		if (at == (uint16_t)cp) {
#ifdef CJK_FONT_IN_FLASH
			// One cell, filled and drawn before the next character is asked for (core/ui/gfx.c, gfx_text)
			static uint8_t cell[CJK_FONT_BYTES];
			if (!font_read(cjk_font_addr + (uint32_t)mid * CJK_FONT_BYTES, cell, CJK_FONT_BYTES))
				return NULL;		// the flash didn't answer: a box, as for a character the face lacks
			return cell;
#else
			return cjk_font_bits + (size_t)mid * CJK_FONT_BYTES;
#endif
		}
		if (at < (uint16_t)cp)
			lo = (uint16_t)(mid + 1u);
		else
			hi = mid;
	}
#endif
	return NULL;			// no table built (CONFIG_CJK_GLYPHS off): the text layer draws a box
}
