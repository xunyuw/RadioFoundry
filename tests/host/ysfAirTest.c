// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
/*
 * Host tests for Fusion's air mode (core/ysf/ysf_air.c; docs/spec/ysf-modem.md 1.7): that the profile a framer
 * receives through a modem chip's FM copy with is the one the chip's fit rules accept, and that a whole transmission
 * survives the air framer and the soft modem's demodulated symbols into the frame layer.
 *
 *   make -C tests/host run-ysf
 *
 * The fit rules are hrc6000_air_fit's (drivers/modem/hrc_common/hrc6000_air.c); that file pulls in the chip's
 * register layer, so the two rules that matter are restated here from modem-hrc6000.md 10.1-10.2 and a test keeps
 * them from drifting: the rate, the deviation band and the sync spacing.
 */

#include <stdio.h>
#include <string.h>
#include "ysf/ysf_air.h"
#include "ysf/ysf_frame.h"
#include "air/air_syms.h"

static int failures = 0;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// The chip's rules (modem-hrc6000.md 10.1), as hrc6000_air_fit applies them to a continuous profile
static void test_profile_fits_the_chip(void)
{
	const air_profile_t *p = &ysf_air_mode.profile;

	CHECK(p->timing == AIR_TIMING_CONTINUOUS, "the chip takes continuous profiles only");
	CHECK(p->sym_rate == 4800, "4800 symbols/s is the chip's own rate, got %u", p->sym_rate);
	CHECK(p->levels == 4 || p->levels == 2, "2 or 4 levels");
	CHECK(p->shape == AIR_SHAPE_C4FM && p->shape_param == 20, "Y 3.2: roll-off 0.2");
	// Transmit: Mod1 moves the deviation linearly and 1050-2400 Hz is the range the pair was checked over (10.3:
	// "above Mod1 0x65 is not checked"). The standard's 2700 Hz sits past it and works - the pair lands at 73/87 of
	// the register's 255 - so the check is that the scale has room, not that the profile stays inside 2400
	CHECK(p->dev_outer_hz >= 1050, "dev_outer_hz %u under the pair's checked floor", p->dev_outer_hz);
	// Receive through the FM copy: 1700-2900 Hz (10.2)
	CHECK(p->dev_outer_hz >= 1700 && p->dev_outer_hz <= 2900,
	      "dev_outer_hz %u outside the copy's receive band", p->dev_outer_hz);
	// The correlator's spacing rule; the copy does not need it, but a profile inside it could use the correlator too
	CHECK(p->sync_every_syms > 0 && p->sync_every_syms <= 1728,
	      "sync spacing %u over the chip's 1728", p->sync_every_syms);
	CHECK(p->sync_every_syms == YSF_FRAME_DIBITS, "the sync opens every frame");
	CHECK(p->nsyncs == 1, "one sync");
	CHECK(p->sync[0].nsyms == YSF_SYNC_DIBITS && p->sync[0].nsyms >= 8
	      && p->sync[0].nsyms <= AIR_SYNC_SYMS_MAX, "the sync's length");
	CHECK(p->sync[0].frame_at == 0, "the sync opens the frame");
	CHECK(ysf_air_mode.frame_syms == YSF_FRAME_DIBITS, "480 symbols a frame");
}

// The correlator cannot take this sync: it needs 24 symbols, all outer, and Fusion's 20 have 7 inner ones. If this
// ever passed, 1.7's whole reason for the soft path would be wrong.
static void test_correlator_rejects_the_sync(void)
{
	const air_sync_t *s = &ysf_air_mode.profile.sync[0];
	int inner = 0;

	for (uint8_t i = 0; i < s->nsyms; i++) {
		uint8_t d = (uint8_t)((s->syms[i / 4] >> (6 - 2 * (i % 4))) & 3u);
		if ((d & 1u) == 0) {
			inner++;
		}
	}
	CHECK(inner == 7, "the sync has 7 inner symbols, got %d", inner);
	CHECK(s->nsyms < AIR_SYNC_SYMS_MAX, "and it is shorter than the correlator's 24");
}

// The sync's bytes are the air bytes of YSF_SYNC: the profile and the frame layer must agree, or a frame handed to
// the receiver starts at the wrong symbol.
static void test_sync_bytes_match_the_frame_layer(void)
{
	const air_sync_t *s = &ysf_air_mode.profile.sync[0];
	uint64_t bits = 0;
	uint64_t want = 0;

	// The profile's first 20 dibits as bits, leftmost first
	for (uint8_t i = 0; i < YSF_SYNC_DIBITS; i++) {
		uint8_t d = (uint8_t)((s->syms[i / 4] >> (6 - 2 * (i % 4))) & 3u);
		bits = (bits << 2) | d;
	}
	// YSF_SYNC's 40 bits, the leftmost first: 20 dibits of two bits each
	for (uint8_t i = 0; i < 20; i++) {
		want = (want << 2) | ((YSF_SYNC >> (38u - 2u * i)) & 3u);
	}
	CHECK(bits == want, "the profile's sync %010llx against YSF_SYNC %010llx",
	      (unsigned long long)bits, (unsigned long long)want);
}

// A whole transmission through the air path: the transmitter's dibits go into the framer's chunks, the framer slices
// the frames out of them with ysf_air_mode, and each frame's symbols go back into the receiver. The header's callsign
// must come out, which proves the profile's geometry and the sync bytes agree with the frame layer end to end.
typedef struct {
	ysf_air_rx_t arx;
	ysf_rx_t *rx;
	int frames;
} e2e_t;

static void e2e_dibits(void *ctx, const uint8_t *d, uint16_t n)
{
	e2e_t *e = ctx;

	e->frames++;
	// The framer hands whole frames, four dibits a byte, the first in bits 7-6 (hal_air.h)
	for (uint16_t i = 0; i < n; i++) {
		ysf_rx_dibit(e->rx, (uint8_t)((d[i / 4] >> (6 - 2 * (i % 4))) & 3u));
	}
}

static void e2e_lost(void *ctx) { (void)ctx; }

static int got_call;

static void e2e_call(void *ctx, const ysf_call_t *c)
{
	(void)ctx;
	(void)c;
	got_call++;
}

static void e2e_frame(void *ctx, const ysf_call_t *c, bool first)
{
	(void)ctx;
	(void)c;
	(void)first;
}

static void e2e_voice(void *ctx, const uint8_t bits[YSF_VOICE_BITS], uint8_t q)
{
	(void)ctx;
	(void)bits;
	(void)q;
}

static void e2e_voice72(void *ctx, const uint8_t frame[9])
{
	(void)ctx;
	(void)frame;
}

static void e2e_end(void *ctx, ysf_end_t why)
{
	(void)ctx;
	(void)why;
}

static void test_a_transmission_through_the_air_path(void)
{
	static ysf_tx_t tx;
	ysf_tx_config_t cfg;
	static ysf_rx_t rx;
	static e2e_t e2e;
	static air_framer_t fr;
	static uint8_t fbuf[YSF_FRAME_DIBITS / 4 + 4];
	static int8_t fsoft[YSF_FRAME_DIBITS + 4];
	static uint8_t syms[8192];
	static uint8_t chunk_hard[AIR_CHUNK_SYMS_MAX / 4];
	static int8_t chunk_soft[AIR_CHUNK_SYMS_MAX];
	size_t nsyms = 0;
	int d;

	memset(&cfg, 0, sizeof cfg);
	ysf_csd_init(&cfg.csd, "K1ABC", "CQCQCQ");
	cfg.headers = 1;
	ysf_tx_start(&tx, &cfg);

	// The transmitter's dibits as air symbols: the frame layer emits the sync and everything after it, and it never
	// runs dry (no voice source: it sends the silence frame), so take a fixed count: the preamble, the header and
	// four voice frames, which is enough for the framer to lock and for the receiver to see the callsigns twice.
	while (nsyms < 6 * YSF_FRAME_DIBITS && (d = ysf_tx_dibit(&tx)) >= 0) {
		syms[nsyms++] = (uint8_t)d;
	}
	CHECK(nsyms == 6 * YSF_FRAME_DIBITS, "six frames' symbols (%zu)", nsyms);

	memset(&e2e, 0, sizeof e2e);
	memset(&rx, 0, sizeof rx);
	static const ysf_rx_cb_t cb = { e2e_frame, e2e_call, e2e_voice, e2e_voice72, e2e_end, NULL };
	ysf_rx_init(&rx, &cb);
	e2e.rx = &rx;
	e2e.arx.dibits = e2e_dibits;
	e2e.arx.lost = e2e_lost;
	e2e.arx.ctx = &e2e;
	got_call = 0;

	air_framer_init(&fr, &ysf_air_mode, &e2e, fbuf, fsoft, YSF_FRAME_DIBITS);
	air_framer_rx_reset(&fr);

	// Feed the symbols the way a provider does: chunks of 144, each with its soft values (clean: +3 is AIR_SOFT_OUTER)
	{
		size_t at = 0;
		bool first = true;
		while (at < nsyms) {
			air_chunk_t c;
			uint16_t n = (uint16_t)(nsyms - at < AIR_CHUNK_SYMS_MAX ? nsyms - at : AIR_CHUNK_SYMS_MAX);

			memset(&c, 0, sizeof c);
			for (uint16_t i = 0; i < n; i++) {
				// 01 = +3, 00 = +1, 10 = -1, 11 = -3 (hal_air.h)
				static const int8_t lv[4] = { 32, 96, -32, -96 };
				air_sym_put(chunk_hard, i, syms[at + i]);
				chunk_soft[i] = lv[syms[at + i] & 3u];
			}
			c.t_us = (uint32_t)((uint64_t)at * 1000000u / 4800u);
			c.nsyms = n;
			c.sync_at = -1;
			c.flags = first ? AIR_CHUNK_GAP : 0;
			c.hard = chunk_hard;
			c.soft = chunk_soft;
			air_framer_rx(&fr, &c);
			at += n;
			first = false;
		}
	}

	CHECK(e2e.frames >= 1, "the framer cut the transmission into frames (%d)", e2e.frames);
	CHECK(got_call >= 1, "the receiver decoded the header's callsign (%d)", got_call);
	ysf_tx_stop(&tx);
}

int main(void)
{
	test_profile_fits_the_chip();
	test_correlator_rejects_the_sync();
	test_sync_bytes_match_the_frame_layer();
	test_a_transmission_through_the_air_path();

	printf("# %s\n", failures == 0 ? "ysfAirTest PASSED" : "ysfAirTest FAILED");
	return failures == 0 ? 0 : 1;
}
