// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Fusion's air mode (docs/spec/ysf-modem.md 1.7): the profile a framer uses for a chip's FM copy, and the two hooks
// that carry its frames to the frame layer. A board with the flat FM paths does not need this: it runs the
// software modem on the discriminator samples (ysf_engine_rx). A board with only the chip's copy (the HR_C6000 ones)
// receives through air_framer + air_softmodem, and this is the mode it registers.
#ifndef RFOUNDRY_YSF_AIR_H
#define RFOUNDRY_YSF_AIR_H

#include <stdint.h>
#include "../air/air_framer.h"

// A frame's symbols go here in air order, the sync first: exactly what ysf_rx_dibit / ysf_engine_rx_dibits take
typedef void (*ysf_air_dibits_t)(void *ctx, const uint8_t *dibits, uint16_t n);
// No sync for longer than the framer's patience: the signal is gone (ysf_rx_t's own lost hook stays unused)
typedef void (*ysf_air_lost_t)(void *ctx);

typedef struct {
	ysf_air_dibits_t dibits;
	ysf_air_lost_t lost;
	void *ctx;
} ysf_air_rx_t;

extern const air_mode_t ysf_air_mode;

#endif
