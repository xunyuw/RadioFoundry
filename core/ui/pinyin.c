// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The pinyin input method's search: a prefix binary search over the sorted readings, then a walk of the characters
// that carry them (core/ui/pinyin.h says what the tables are). No RAM: the tables are read through the flash bus and a
// page of candidates is copied into the caller's buffer.
//
// Two shapes, as for the CJK face (core/ui/cjk_font.c): the tables in this image (a board with room to spare), or in
// the data flash at CONFIG_PINYIN_ADDR (a board that keeps a whole vocabulary there, tools/pinyin_table.py --out-bin
// writes it). The flash shape costs one small read per probe of the search and one per character walked.
#include <stddef.h>
#include <string.h>
#include "pinyin.h"
#if CONFIG_PINYIN_IN_FLASH
#include "board.h"
#include "storage/storage.h"
#endif

#if CONFIG_PINYIN

extern const uint16_t pinyin_syllable_count;
extern const uint16_t pinyin_entry_count;

#if CONFIG_PINYIN_IN_FLASH
// One reading, or one entry, read from the data flash
static bool pinyin_read(uint32_t addr, void *buf, uint32_t len)
{
	if (board.flash == NULL || board.flash->read == NULL)
		return false;
	storage_lock();
	bool ok = board.flash->read(addr, buf, len);
	storage_unlock();
	return ok;
}

static bool syllable_at(uint16_t i, char out[PINYIN_SYLLABLE_BYTES])
{
	return pinyin_read(pinyin_addr + (uint32_t)i * PINYIN_SYLLABLE_BYTES, out, PINYIN_SYLLABLE_BYTES);
}

static bool entry_at(uint16_t i, uint16_t *cp, uint16_t *syllable)
{
	uint8_t raw[4];
	// The readings come first in the flash image, then the entries
	uint32_t base = pinyin_addr + (uint32_t)pinyin_syllable_count * PINYIN_SYLLABLE_BYTES;
	if (!pinyin_read(base + (uint32_t)i * 4u, raw, sizeof raw))
		return false;
	*cp = (uint16_t)(raw[0] | (raw[1] << 8));
	*syllable = (uint16_t)(raw[2] | (raw[3] << 8));
	return true;
}
#else
extern const char pinyin_syllable[][PINYIN_SYLLABLE_BYTES];
extern const pinyin_entry_t pinyin_entry[];

static bool syllable_at(uint16_t i, char out[PINYIN_SYLLABLE_BYTES])
{
	memcpy(out, pinyin_syllable[i], PINYIN_SYLLABLE_BYTES);
	return true;
}

static bool entry_at(uint16_t i, uint16_t *cp, uint16_t *syllable)
{
	*cp = pinyin_entry[i].cp;
	*syllable = pinyin_entry[i].syllable;
	return true;
}
#endif

// The first reading that is not before `prefix` in sorted order
static uint16_t syllable_lower_bound(const char *prefix)
{
	uint16_t lo = 0, hi = pinyin_syllable_count;
	while (lo < hi) {
		uint16_t mid = (uint16_t)((lo + hi) / 2u);
		char field[PINYIN_SYLLABLE_BYTES];
		if (!syllable_at(mid, field))
			return lo = hi;				// the flash didn't answer: no candidates
		if (strncmp(field, prefix, PINYIN_SYLLABLE_BYTES) < 0)
			lo = (uint16_t)(mid + 1u);
		else
			hi = mid;
	}
	return lo;
}

// The first entry of a reading: the table is sorted by (reading, code point)
static uint16_t syllable_first_entry(uint16_t syllable)
{
	uint16_t lo = 0, hi = pinyin_entry_count;
	while (lo < hi) {
		uint16_t mid = (uint16_t)((lo + hi) / 2u);
		uint16_t cp, syl;
		if (!entry_at(mid, &cp, &syl))
			return lo = hi;
		if (syl < syllable)
			lo = (uint16_t)(mid + 1u);
		else
			hi = mid;
	}
	return lo;
}

uint16_t pinyin_candidates(const char *prefix, uint16_t from, uint16_t *out, uint16_t cap, uint16_t *next)
{
	*next = 0;
	if (prefix == NULL || prefix[0] == '\0' || cap == 0)
		return 0;
	size_t plen = strlen(prefix);
	if (plen > PINYIN_LETTERS_MAX)
		return 0;

	uint16_t first_syl = syllable_lower_bound(prefix);
	uint16_t i = syllable_first_entry(first_syl);
	uint16_t skipped = 0, n = 0;
	for (; i < pinyin_entry_count; i++) {
		uint16_t cp, syl;
		if (!entry_at(i, &cp, &syl))
			return n;
		char field[PINYIN_SYLLABLE_BYTES];
		if (!syllable_at(syl, field))
			return n;
		if (strncmp(field, prefix, plen) != 0)
			break;				// past the readings that start with it
		if (skipped < from) {
			skipped++;
			continue;
		}
		if (n >= cap) {
			*next = (uint16_t)(from + n);	// more than a page: where the next one starts
			return n;
		}
		out[n++] = cp;
	}
	return n;
}

// The digits a letter sits on, a phone keypad's (the editor's pinyin_keys): 'a' - 'z' -> 2..9
static uint8_t letter_digit(char c)
{
	static const char rows[] = "22233344455566677778889999";
	if (c < 'a' || c > 'z')
		return 0;
	return (uint8_t)(rows[c - 'a'] - '0');
}

// Whether a reading's first `n` letters sit on these digits: T9, where a key means any of its three or four letters
static bool syllable_fits(const char *field, const uint8_t *digits, uint8_t n)
{
	for (uint8_t i = 0; i < n; i++) {
		if (field[i] == '\0' || letter_digit(field[i]) != digits[i])
			return false;
	}
	return true;
}

// The characters a run of keypad digits can spell (CONFIG_PINYIN, T9): every reading whose first n letters sit on
// those digits, in table order. The table is sorted by the letters, not by the digits, so this walks every reading
// rather than bisecting: 400 readings, each a few byte comparisons, is cheaper than a second index.
uint16_t pinyin_candidates_t9(const uint8_t *digits, uint8_t n, uint16_t from, uint16_t *out, uint16_t cap,
			      uint16_t *next)
{
	uint16_t skipped = 0, got = 0;

	*next = 0;
	if (digits == NULL || n == 0 || n > PINYIN_LETTERS_MAX || cap == 0)
		return 0;
	for (uint16_t syl = 0; syl < pinyin_syllable_count; syl++) {
		char field[PINYIN_SYLLABLE_BYTES];
		if (!syllable_at(syl, field))
			return got;
		if (!syllable_fits(field, digits, n))
			continue;
		uint16_t first = syllable_first_entry(syl);
		uint16_t end = first;
		while (end < pinyin_entry_count) {
			uint16_t cp, s2;
			if (!entry_at(end, &cp, &s2))
				return got;
			if (s2 != syl)
				break;
			if (skipped < from) {
				skipped++;
			} else if (got >= cap) {
				*next = (uint16_t)(from + got);
				return got;
			} else {
				out[got++] = cp;
			}
			end++;
		}
	}
	return got;
}

uint16_t pinyin_count(void)
{
	return pinyin_entry_count;
}

#endif
