//
// bcrush - Example of CRUSH compression with BriefLZ algorithms
//
// C packer
//
// Copyright (c) 2018-2020 Joergen Ibsen
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//   1. The origin of this software must not be misrepresented; you must
//      not claim that you wrote the original software. If you use this
//      software in a product, an acknowledgment in the product
//      documentation would be appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must
//      not be misrepresented as being the original software.
//
//   3. This notice may not be removed or altered from any source
//      distribution.
//

#include "crush.h"
#include "crush_internal.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#if _MSC_VER >= 1400
#  include <intrin.h>
#  define CRUSH_BUILTIN_MSVC
#elif defined(__clang__) && defined(__has_builtin)
#  if __has_builtin(__builtin_clz)
#    define CRUSH_BUILTIN_GCC
#  endif
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#  define CRUSH_BUILTIN_GCC
#endif

// Number of bits of hash to use for lookup.
//
// The size of the lookup table (and thus workmem) depends on this.
//
// Values between 10 and 18 work well. Lower values generally make compression
// speed faster but ratio worse. The default value 17 (128k entries) is a
// compromise.
//
#ifndef CRUSH_HASH_BITS
#  define CRUSH_HASH_BITS 17
#endif

#define LOOKUP_SIZE (1UL << CRUSH_HASH_BITS)

#define WORKMEM_SIZE (LOOKUP_SIZE * sizeof(uint32_t))

#define NO_MATCH_POS ((uint32_t) -1)

struct lsb_bitwriter {
	unsigned char *next_out;
	uint32_t tag;
	int msb;
};

static void
lbw_init(struct lsb_bitwriter *lbw, unsigned char *dst)
{
	lbw->next_out = dst;
	lbw->tag = 0;
	lbw->msb = 0;
}

static unsigned char*
lbw_finalize(struct lsb_bitwriter *lbw)
{
	// Write bytes until no bits left in tag
	while (lbw->msb > 0) {
		*lbw->next_out++ = lbw->tag;
		lbw->tag >>= 8;
		lbw->msb -= 8;
	}

	return lbw->next_out;
}

static void
lbw_flush(struct lsb_bitwriter *lbw, int num) {
	assert(num >= 0 && num <= 32);

	// Write bytes until at least num bits free
	while (lbw->msb > 32 - num) {
		*lbw->next_out++ = lbw->tag;
		lbw->tag >>= 8;
		lbw->msb -= 8;
	}

	assert(lbw->msb >= 0);
}

static void
lbw_putbits_no_flush(struct lsb_bitwriter *lbw, uint32_t bits, int num) {
	assert(num >= 0 && num <= 32 - lbw->msb);
	assert((bits & (~0ULL << num)) == 0);

	// Add bits to tag
	lbw->tag |= bits << lbw->msb;
	lbw->msb += num;
}

static void
lbw_putbits(struct lsb_bitwriter *lbw, uint32_t bits, int num) {
	lbw_flush(lbw, num);
	lbw_putbits_no_flush(lbw, bits, num);
}

static int
crush_log2(unsigned long n)
{
	assert(n > 0);

#if defined(CRUSH_BUILTIN_MSVC)
	unsigned long msb_pos;
	_BitScanReverse(&msb_pos, n);
	return (int) msb_pos;
#elif defined(CRUSH_BUILTIN_GCC)
	return (int) sizeof(n) * CHAR_BIT - 1 - __builtin_clzl(n);
#else
	int bits = 0;

	while (n >>= 1) {
		++bits;
	}

	return bits;
#endif
}

// Hash three bytes starting a p.
//
// This is Fibonacci hashing, also known as Knuth's multiplicative hash. The
// constant is a prime close to 2^32/phi.
//
static unsigned long
crush_hash3_bits(const unsigned char *p, int bits)
{
	assert(bits > 0 && bits <= 32);

	uint32_t val = (uint32_t) p[0]
	             | ((uint32_t) p[1] << 8)
	             | ((uint32_t) p[2] << 16);

	return (val * UINT32_C(2654435761)) >> (32 - bits);
}

static unsigned long
crush_match_cost(unsigned long pos, unsigned long len)
{
	unsigned long cost = 1;

	const unsigned long l = len - MIN_MATCH;

	if (l < A) {
		cost += 1 + A_BITS;
	}
	else if (l < B) {
		cost += 2 + B_BITS;
	}
	else if (l < C) {
		cost += 3 + C_BITS;
	}
	else if (l < D) {
		cost += 4 + D_BITS;
	}
	else if (l < E) {
		cost += 5 + E_BITS;
	}
	else {
		cost += 5 + F_BITS;
	}

	cost += SLOT_BITS;

	if (pos >= (2UL << (W_BITS - NUM_SLOTS))) {
		cost += crush_log2(pos);
	}
	else {
		cost += W_BITS - (NUM_SLOTS - 1);
	}

	return cost;
}

unsigned long
crush_max_packed_size(unsigned long src_size)
{
	return src_size + src_size / 8 + 64;
}

// Include compression algorithms used by crush_pack_level
#include "crush_btparse.h"
#include "crush_leparse.h"

size_t
crush_workmem_size_level(size_t src_size, int level)
{
	switch (level) {
	case 5:
	case 6:
	case 7:
		return crush_leparse_workmem_size(src_size);
	case 8:
	case 9:
	case 10:
		return crush_btparse_workmem_size(src_size);
	default:
		return (size_t) -1;
	}
}

unsigned long
crush_pack_level(const void *src, void *dst, unsigned long src_size,
                 void *workmem, int level)
{
	switch (level) {
	case 5:
		return crush_pack_leparse(src, dst, src_size, workmem, 1, 16);
	case 6:
		return crush_pack_leparse(src, dst, src_size, workmem, 8, 32);
	case 7:
		return crush_pack_leparse(src, dst, src_size, workmem, 64, 64);
	case 8:
		return crush_pack_btparse(src, dst, src_size, workmem, 16, 96);
	case 9:
		return crush_pack_btparse(src, dst, src_size, workmem, 32, 224);
	case 10:
		return crush_pack_btparse(src, dst, src_size, workmem, ULONG_MAX, ULONG_MAX);
	default:
		return CRUSH_ERROR;
	}
}

// clang -g -O1 -fsanitize=fuzzer,address -DCRUSH_FUZZING crush.c crush_depack.c
#if defined(CRUSH_FUZZING)
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRUSH_FUZZ_LEVEL
#  define CRUSH_FUZZ_LEVEL 5
#endif

extern int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size > 64 * 1024 * 1024UL) { return 0; }
	void *workmem = malloc(crush_workmem_size_level(size, CRUSH_FUZZ_LEVEL));
	void *packed = malloc(crush_max_packed_size(size));
	void *depacked = malloc(size);
	if (!workmem || !packed || !depacked) { abort(); }
	unsigned long packed_size = crush_pack_level(data, packed, size, workmem, CRUSH_FUZZ_LEVEL);
	crush_depack(packed, depacked, size);
	if (memcmp(data, depacked, size)) { abort(); }
	free(depacked);
	free(packed);
	free(workmem);
	return 0;
}
#endif
