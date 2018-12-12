/*
 * bcrush - Example of CRUSH compression with BriefLZ algorithms
 *
 * C depacker
 *
 * Copyright (c) 2018 Joergen Ibsen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must
 *      not claim that you wrote the original software. If you use this
 *      software in a product, an acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must
 *      not be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 */

#include "crush.h"
#include "crush_internal.h"

#include <assert.h>
#include <stdint.h>

struct lsb_bitreader {
	const unsigned char *src;
	uint32_t tag;
	int msb;
};

static void
lbr_init(struct lsb_bitreader *lbr, const unsigned char *src)
{
	lbr->src = src;
	lbr->tag = 0;
	lbr->msb = 0;
}

static void
lbr_refill(struct lsb_bitreader *lbr, int num)
{
	assert(num >= 0 && num <= 32);

	// Read bytes until at least num bits available
	while (lbr->msb < num) {
		lbr->tag |= (uint32_t) *lbr->src++ << lbr->msb;
		lbr->msb += 8;
	}

	assert(lbr->msb <= 32);
}

static uint32_t
lbr_getbits_no_refill(struct lsb_bitreader *lbr, int num)
{
	assert(num >= 0 && num <= lbr->msb);

	// Get bits from tag
	uint32_t bits = lbr->tag & ((1ULL << num) - 1);

	// Remove bits from tag
	lbr->tag >>= num;
	lbr->msb -= num;

	return bits;
}

static uint32_t
lbr_getbits(struct lsb_bitreader *lbr, int num)
{
	lbr_refill(lbr, num);
	return lbr_getbits_no_refill(lbr, num);
}

unsigned long
crush_depack(const void *src, void *dst, unsigned long depacked_size)
{
	struct lsb_bitreader lbr;
	unsigned char *out = (unsigned char *) dst;
	unsigned long dst_size = 0;

	lbr_init(&lbr, (const unsigned char *) src);

	/* Main decompression loop */
	while (dst_size < depacked_size) {
		if (lbr_getbits(&lbr, 1)) {
			unsigned long len;
			unsigned long mlog;
			unsigned long mpos;
			unsigned long offs;

			/* Decode match length */
			if (lbr_getbits(&lbr, 1)) {
				len = lbr_getbits(&lbr, A_BITS);
			}
			else if (lbr_getbits(&lbr, 1)) {
				len = lbr_getbits(&lbr, B_BITS) + A;
			}
			else if (lbr_getbits(&lbr, 1)) {
				len = lbr_getbits(&lbr, C_BITS) + B;
			}
			else if (lbr_getbits(&lbr, 1)) {
				len = lbr_getbits(&lbr, D_BITS) + C;
			}
			else if (lbr_getbits(&lbr, 1)) {
				len = lbr_getbits(&lbr, E_BITS) + D;
			}
			else {
				len = lbr_getbits(&lbr, F_BITS) + E;
			}

			/* Decode match offset */
			mlog = lbr_getbits(&lbr, SLOT_BITS) + (W_BITS - NUM_SLOTS);
			offs = mlog > (W_BITS - NUM_SLOTS)
			     ? lbr_getbits(&lbr, mlog) + (1 << mlog)
			     : lbr_getbits(&lbr, W_BITS - (NUM_SLOTS - 1));

			if (++offs > dst_size) {
				return CRUSH_ERROR;
			}

			mpos = dst_size - offs;

			/* Copy match */
			out[dst_size++] = out[mpos++];
			out[dst_size++] = out[mpos++];
			out[dst_size++] = out[mpos++];
			while (len-- != 0) {
				out[dst_size++] = out[mpos++];
			}
		}
		else {
			/* Copy literal */
			out[dst_size++] = lbr_getbits(&lbr, 8);
		}
	}

	/* Return decompressed size */
	return dst_size;
}
