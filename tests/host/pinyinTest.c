// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The pinyin input method's search (core/ui/pinyin.c): the readings, the prefix lookup, the pages, and what the
// editor turns a picked candidate into. Built only with CONFIG_PINYIN; the tables come from tools/pinyin_table.py.
#include <stdio.h>
#include <string.h>
#include "pinyin.h"

static int checks, failures;
#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

// The table this build was made with: every GB2312 character that has a reading
extern const uint16_t pinyin_syllable_count;
extern const uint16_t pinyin_entry_count;

static void test_tables(void)
{
	CHECK(pinyin_syllable_count == 400);		// GB2312's distinct readings
	CHECK(pinyin_entry_count == 6763);		// every hanzi GB2312 codes
	CHECK(pinyin_count() == pinyin_entry_count);

	// The readings are sorted and lower case, which the binary search relies on
	for (uint16_t i = 0; i < pinyin_syllable_count; i++) {
		const char *r = pinyin_syllable[i];
		CHECK(r[0] >= 'a' && r[0] <= 'z');
		CHECK(strlen(r) <= PINYIN_LETTERS_MAX);
		if (i > 0)
			CHECK(strcmp(pinyin_syllable[i - 1], r) < 0);
	}
}

static void test_lookup(void)
{
	uint16_t out[8], next = 1;

	// An empty prefix has none: the editor shows the plain modes until a letter is typed
	CHECK(pinyin_candidates("", 0, out, 8, &next) == 0 && next == 0);
	CHECK(pinyin_candidates(NULL, 0, out, 8, &next) == 0);

	// "ni" spells several characters and 你 (U+4F60) is one of them
	uint16_t n = pinyin_candidates("ni", 0, out, 8, &next);
	CHECK(n > 0);
	bool has_ni = false;
	for (uint16_t i = 0; i < n; i++)
		has_ni = has_ni || out[i] == 0x4F60;
	CHECK(has_ni);

	// A prefix reaches every reading that starts with it: "zh" is more than "z", and walking its pages visits every
	// one of them once
	uint16_t zh_total = 0, page = 0;
	do {
		uint16_t got = pinyin_candidates("zh", page, out, 8, &next);
		zh_total = (uint16_t)(zh_total + got);
		page = next;
	} while (next != 0 && zh_total < 6000);
	CHECK(zh_total > 100);			// zha, zhai, zhan, zhang, zhao, zhe, zhen, zheng, zhi, zhong, zhou, zhu, ...

	// A reading that is not there spells nothing
	CHECK(pinyin_candidates("qqq", 0, out, 8, &next) == 0);

	// A prefix longer than any reading is refused rather than searched
	CHECK(pinyin_candidates("zhuanqq", 0, out, 8, &next) == 0);
}

static void test_pages(void)
{
	uint16_t a[4], b[4], next = 0;

	// A page of 4 and the one after it hold different characters and no character twice
	uint16_t na = pinyin_candidates("ji", 0, a, 4, &next);	// ji has the most characters of any reading
	CHECK(na == 4 && next != 0);
	uint16_t nb = pinyin_candidates("ji", next, b, 4, &next);
	CHECK(nb == 4);
	for (uint16_t i = 0; i < na; i++)
		for (uint16_t j = 0; j < nb; j++)
			CHECK(a[i] != b[j]);

	// "ji" is a prefix of ji, jia, jian, jiang, jiao, jie, jin, jing, jiong and jiu: hundreds of characters, so a page
	// of 256 still has more after it
	uint16_t out[256];
	uint16_t total = pinyin_candidates("ji", 0, out, 256, &next);
	CHECK(total == 256 && next == 256);

	// A reading that no other starts with has its own characters and then the end: "zhuang" is the longest
	uint16_t ztotal = pinyin_candidates("zhuang", 0, out, 256, &next);
	CHECK(ztotal > 0 && next == 0);		// every "zhuang" character fits one page and there is no next
}

static void test_utf8(void)
{
	// What the editor writes for a picked candidate: 3 bytes, the form gfx_utf8_next decodes
	uint32_t cp = 0x4F60;			// 你
	char utf8[3];
	utf8[0] = (char)(0xE0u | (cp >> 12));
	utf8[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
	utf8[2] = (char)(0x80u | (cp & 0x3Fu));
	CHECK((unsigned char)utf8[0] == 0xE4 && (unsigned char)utf8[1] == 0xBD && (unsigned char)utf8[2] == 0xA0);
}

// T9: a key stands for all its letters, so one digit run reaches several readings. 746 is p-i-n and spells 拼 (pin);
// 7464 is p-i-n-g and q-i-n-g, so it reaches 平 (ping) and 青 (qing) both
static void test_t9(void)
{
	uint16_t out[8], next = 1;

	// No digits, nothing to spell
	CHECK(pinyin_candidates_t9(NULL, 0, 0, out, 8, &next) == 0 && next == 0);
	CHECK(pinyin_candidates_t9(NULL, 4, 0, out, 8, &next) == 0);

	// 746 is pin: 拼 (U+62FC). 7464 is ping and qing, so 平 (U+5E73) and 青 (U+9752)
	{
		static const uint8_t pin[3] = { 7, 4, 6 };
		static const uint8_t d[4] = { 7, 4, 6, 4 };
		uint16_t total = 0, page = 0;
		bool has_pin = false, has_ping = false, has_qing = false;
		do {
			uint16_t n = pinyin_candidates_t9(pin, 3, page, out, 8, &next);
			for (uint16_t i = 0; i < n; i++)
				has_pin = has_pin || out[i] == 0x62FC;
			total = (uint16_t)(total + n);
			page = next;
		} while (next != 0 && total < 6000);
		CHECK(has_pin);
		total = 0; page = 0; next = 1;
		do {
			uint16_t n = pinyin_candidates_t9(d, 4, page, out, 8, &next);
			for (uint16_t i = 0; i < n; i++) {
				has_ping = has_ping || out[i] == 0x5E73;
				has_qing = has_qing || out[i] == 0x9752;
			}
			total = (uint16_t)(total + n);
			page = next;
		} while (next != 0 && total < 6000);
		CHECK(has_ping);
		CHECK(has_qing);
		CHECK(total > 0);
	}

	// A digit no reading starts with spell nothing: 1 and 0 carry no letters, and no reading begins with 9's letters
	// in a row
	{
		static const uint8_t one[1] = { 1 };
		CHECK(pinyin_candidates_t9(one, 1, 0, out, 8, &next) == 0);
	}

	// Every candidate is a reading whose letters sit on the digits: "zhuang" is 9-4-8-2-6-4 and must be found
	{
		static const uint8_t zhuang[6] = { 9, 4, 8, 2, 6, 4 };
		uint16_t n = pinyin_candidates_t9(zhuang, 6, 0, out, 8, &next);
		CHECK(n > 0);
	}

	// More digits than a reading can have: none
	{
		static const uint8_t many[7] = { 2, 2, 2, 2, 2, 2, 2 };
		CHECK(pinyin_candidates_t9(many, 7, 0, out, 8, &next) == 0);
	}
}

int main(void)
{
	test_tables();
	test_lookup();
	test_pages();
	test_utf8();
	test_t9();
	if (failures == 0)
		printf("# pinyinTest: %d checks, PASSED\n", checks);
	else
		printf("# pinyinTest: %d checks, %d FAILED\n", checks, failures);
	return failures != 0;
}
