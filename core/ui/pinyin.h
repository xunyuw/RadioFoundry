// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The pinyin input method (CONFIG_PINYIN): a radio keypad types Latin letters, and this turns a prefix of them into
// the characters a Chinese name can hold. The tables come from tools/pinyin_table.py; this file is the search over
// them and stays small on purpose (core/ui/pinyin.c holds the walk).
//
// The editor (core/ui/ui_menus.c, text_key_fn) collects the letters into its own buffer while the mode is PINYIN, asks
// pinyin_candidates() for the characters those letters can spell, shows them on a line of their own, and writes the
// one picked as its 3-byte UTF-8 into the name.
#ifndef UI_PINYIN_H
#define UI_PINYIN_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// A reading in a fixed field: the longest is "zhuang" (6 letters) plus its NUL, padded to 8
#define PINYIN_SYLLABLE_BYTES	8
// The most letters a reading can have before the editor stops taking them: "zhuang"
#define PINYIN_LETTERS_MAX	6

#if CONFIG_PINYIN

// One character with a reading: its code point and the index of its reading in pinyin_syllable
typedef struct {
	uint16_t cp;
	uint16_t syllable;
} pinyin_entry_t;

// The generated table (core/ui/pinyin_data.c) carries both shapes and CONFIG_PINYIN_IN_FLASH picks one: the tables in
// this image, or their address in the data flash
#if CONFIG_PINYIN_IN_FLASH
extern const uint32_t pinyin_addr;
#else
extern const char pinyin_syllable[][PINYIN_SYLLABLE_BYTES];
extern const pinyin_entry_t pinyin_entry[];
#endif
extern const uint16_t pinyin_syllable_count;
extern const pinyin_entry_t pinyin_entry[];
extern const uint16_t pinyin_entry_count;

// The characters whose reading starts with `prefix`, from `from` (0 for the first page): their code points into `out`,
// up to `cap`, and the count on this page. *next is where the following page starts, or 0 when there is none.
// An empty prefix has no candidates (the editor shows the plain modes until a letter is typed).
uint16_t pinyin_candidates(const char *prefix, uint16_t from, uint16_t *out, uint16_t cap, uint16_t *next);

// How many characters the whole GB2312 set has a reading for: the size of the table, for tests and the UI's counts
uint16_t pinyin_count(void);

#endif

#endif
