// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Fusion (C4FM) on the air interface (docs/spec/ysf-modem.md 1.7): the mode's geometry for a framer that receives
// through a modem chip's FM copy, and the receive callback that hands the symbols to the frame layer. Nothing here
// knows a board: a provider that fits this profile serves it, a board whose glue runs it turns CONFIG_YSF on.
//
// The correlator of an HR_C6000 never fits this profile (its sync is 20 symbols and 7 of them inner, the correlator
// takes only 24 all-outer ones), so receive is the chip's FM copy into the soft modem, as P25's and NXDN's is there.
#include <string.h>
#include "ysf/ysf_air.h"
#include "ysf/ysf_frame.h"
#include "../../hal/hal_radio.h"

// The frame's sync, packed four dibits a byte, the first on air in bits 7-6 (hal_air.h): D4 71 C9 63 4D is
// YSF_SYNC's 20 dibits as they sit on air, the same bytes the frame carries.
static void rx_frame(const air_frame_t *f, void *ctx)
{
	ysf_air_rx_t *rx = ctx;

	if (rx->dibits != NULL) {
		// The frame's symbols are the sync and the frame it opens, air order, so the frame layer's own search finds
		// the sync where it belongs. A frame placed by counting (no sync seen) is fed too: the layer notices the
		// miss and its FICH check decides, as it does on a symbol path.
		rx->dibits(rx->ctx, f->hard, f->nsyms);
	}
}

static void rx_lost(void *ctx)
{
	ysf_air_rx_t *rx = ctx;

	if (rx->lost != NULL) {
		rx->lost(rx->ctx);
	}
}

const air_mode_t ysf_air_mode = {
	.name = "YSF",
	.profile = {
		.sym_rate = 4800,
		.levels = 4,
		.shape = AIR_SHAPE_C4FM,	// Y 3.2 table 3-1: the same pulse as DMR, roll-off 0.2
		.shape_param = 20,
		// 1.6: a Yaesu radio's wide setting, +-900 / +-2700 Hz. The outer level is the one a stock radio decides its
		// symbols against, so a transmission at any other deviation reads as the wrong levels there: 2700, the standard,
		// inside the chip's checked range (1050-2400 Hz, modem-hrc6000.md 10.3) for the pair and the correlator's
		// receive band (1700-2900 Hz)
		.dev_outer_hz = 2700,
		.timing = AIR_TIMING_CONTINUOUS,
		.bw = RADIO_BW_25K,		// 1.3
		.slot_syms = 0,
		.sync_every_syms = YSF_FRAME_DIBITS,	// the sync opens every frame
		.sync_any_syms = YSF_FRAME_DIBITS,
		.idle = 0x5F,			// a terminator's first symbols, so a run-dry never forges a sync
		.nsyncs = 1,
		.sync = { {
			.id = 0,
			.nsyms = YSF_SYNC_DIBITS,
			.max_errs = 3,		// YSF_SYNC_SEARCH_ERRORS (ysf_frame.c)
			.flags = 0,
			.frame_at = 0,
			.syms = { 0xD4, 0x71, 0xC9, 0x63, 0x4D },
		} },
	},
	.frame_syms = YSF_FRAME_DIBITS,
	.head_syms = 0,			// the FICH behind the sync is the confirmation, in the frame layer
	.lost_frames = 2,
	.head = NULL,
	.rx_frame = rx_frame,
	.rx_lost = rx_lost,
};
