//
// bcrush - Example of CRUSH compression with BriefLZ algorithms
//
// Backwards dynamic programming parse with left-extension of matches
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

#ifndef CRUSH_LEPARSE_H_INCLUDED
#define CRUSH_LEPARSE_H_INCLUDED

static unsigned long
crush_leparse_workmem_size(unsigned long src_size)
{
	return (LOOKUP_SIZE < 2 * src_size ? 3 * src_size : src_size + LOOKUP_SIZE)
	     * sizeof(unsigned long);
}

static unsigned long
crush_pack_leparse(const void *src, void *dst, unsigned long src_size, void *workmem,
                   const unsigned long max_depth, const unsigned long accept_len)
{
	struct lsb_bitwriter lbw;
	const unsigned char *const in = (const unsigned char *) src;
	const unsigned long last_match_pos = src_size > 3 ? src_size - 3 : 0;

	// Check for empty input
	if (src_size == 0) {
		return 0;
	}

	lbw_init(&lbw, (unsigned char *) dst);

	if (src_size < 4) {
		for (unsigned long i = 0; i < src_size; ++i) {
			lbw_putbits(&lbw, (uint32_t) in[i] << 1, 9);
		}
		goto finalize;
	}

	// With a bit of careful ordering we can fit in 3 * src_size words.
	//
	// The idea is that the lookup is only used in the first phase to
	// build the hash chains, so we overlap it with mpos and mlen.
	// Also, since we are using prev from right to left in phase two,
	// and that is the order we fill in cost, we can overlap these.
	//
	// One detail is that we actually use src_size + 1 elements of cost,
	// but we put mpos after it, where we do not need the first element.
	//
	unsigned long *const prev = (unsigned long *) workmem;
	unsigned long *const mpos = prev + src_size;
	unsigned long *const mlen = mpos + src_size;
	unsigned long *const cost = prev;
	unsigned long *const lookup = mpos;

	// Phase 1: Build hash chains
	const int bits = 2 * src_size < LOOKUP_SIZE ? CRUSH_HASH_BITS : crush_log2(src_size);

	// Initialize lookup
	for (unsigned long i = 0; i < (1UL << bits); ++i) {
		lookup[i] = NO_MATCH_POS;
	}

	// Build hash chains in prev
	if (last_match_pos > 0) {
		for (unsigned long i = 0; i <= last_match_pos; ++i) {
			const unsigned long hash = crush_hash3_bits(&in[i], bits);
			prev[i] = lookup[hash];
			lookup[hash] = i;
		}
	}

	// Initialize last two positions as literals
	mlen[src_size - 2] = 1;
	mlen[src_size - 1] = 1;

	cost[src_size - 2] = 18;
	cost[src_size - 1] = 9;
	cost[src_size] = 0;

	// Phase 2: Find lowest cost path from each position to end
	for (unsigned long cur = last_match_pos; cur > 0; --cur) {
		// Since we updated prev to the end in the first phase, we
		// do not need to hash, but can simply look up the previous
		// position directly.
		unsigned long pos = prev[cur];

		assert(pos == NO_MATCH_POS || pos < cur);

		// Start with a literal
		cost[cur] = cost[cur + 1] + 9;
		mlen[cur] = 1;

		unsigned long max_len = MIN_MATCH - 1;

		const unsigned long len_limit = src_size - cur > MAX_MATCH ? MAX_MATCH : src_size - cur;
		unsigned long num_chain = max_depth;

		// Go through the chain of prev matches
		for (; pos != NO_MATCH_POS && num_chain--; pos = prev[pos]) {
			// Limit offset to W_SIZE
			if (cur - pos > W_SIZE) {
				break;
			}

			// The CRUSH packer drops length 3 matches further
			// away than TOO_FAR (64k). The actual point at which
			// a match is longer than 3 literals is 1M, so this
			// might be a heuristic to find better matches at the
			// next position. At any rate, it is not uses in the
			// depacker, so I left it out here because that
			// improves ratio by a tiny bit.
/*
			// Minimum match length 4 for offset > 64k
			if (max_len == MIN_MATCH - 1 && cur - pos > TOO_FAR) {
				max_len = MIN_MATCH;
			}
*/
			unsigned long len = 0;

			// If next byte matches, so this has a chance to be a longer match
			if (max_len < len_limit && in[pos + max_len] == in[cur + max_len]) {
				// Find match len
				while (len < len_limit && in[pos + len] == in[cur + len]) {
					++len;
				}
			}

			// Extend current match if possible
			//
			// Note that we are checking matches in order from the
			// closest and back. This means for a match further
			// away, the encoding of all lengths up to the current
			// max length will always be longer or equal, so we need
			// only consider the extension.
			if (len > max_len) {
				unsigned long min_cost = ULONG_MAX;
				unsigned long min_cost_len = MIN_MATCH - 1;

				// Find lowest cost match length
				for (unsigned long i = max_len + 1; i <= len; ++i) {
					unsigned long match_cost = crush_match_cost(cur - pos - 1, i);
					assert(match_cost < ULONG_MAX - cost[cur + i]);
					unsigned long cost_here = match_cost + cost[cur + i];

					if (cost_here < min_cost) {
						min_cost = cost_here;
						min_cost_len = i;
					}
				}

				max_len = len;

				// Update cost if cheaper
				if (min_cost < cost[cur]) {
					cost[cur] = min_cost;
					mpos[cur] = pos;
					mlen[cur] = min_cost_len;

					// Left-extend current match if possible
					if (pos > 0 && in[pos - 1] == in[cur - 1] && min_cost_len < MAX_MATCH) {
						do {
							--cur;
							--pos;
							++min_cost_len;
							unsigned long match_cost = crush_match_cost(cur - pos - 1, min_cost_len);
							assert(match_cost < ULONG_MAX - cost[cur + min_cost_len]);
							unsigned long cost_here = match_cost + cost[cur + min_cost_len];
							cost[cur] = cost_here;
							mpos[cur] = pos;
							mlen[cur] = min_cost_len;
						} while (pos > 0 && in[pos - 1] == in[cur - 1] && min_cost_len < MAX_MATCH);
						break;
					}
				}
			}

			if (len >= accept_len || len == len_limit) {
				break;
			}
		}
	}

	mpos[0] = 0;
	mlen[0] = 1;

	// Phase 3: Output compressed data, following lowest cost path
	for (unsigned long i = 0; i < src_size; i += mlen[i]) {
		if (mlen[i] == 1) {
			lbw_putbits(&lbw, (uint32_t) in[i] << 1, 9);
		}
		else {
			const unsigned long offs = i - mpos[i] - 1;

			lbw_putbits(&lbw, 1, 1);

			const unsigned long l = mlen[i] - MIN_MATCH;

			if (l < A) {
				lbw_putbits(&lbw, 1UL, 1);
				lbw_putbits(&lbw, l, A_BITS);
			}
			else if (l < B) {
				lbw_putbits(&lbw, 1UL << 1, 2);
				lbw_putbits(&lbw, l - A, B_BITS);
			}
			else if (l < C) {
				lbw_putbits(&lbw, 1UL << 2, 3);
				lbw_putbits(&lbw, l - B, C_BITS);
			}
			else if (l < D) {
				lbw_putbits(&lbw, 1UL << 3, 4);
				lbw_putbits(&lbw, l - C, D_BITS);
			}
			else if (l < E) {
				lbw_putbits(&lbw, 1UL << 4, 5);
				lbw_putbits(&lbw, l - D, E_BITS);
			}
			else {
				lbw_putbits(&lbw, 0, 5);
				lbw_putbits(&lbw, l - E, F_BITS);
			}

			unsigned long mlog = W_BITS - NUM_SLOTS;

			while (offs >= (2UL << mlog)) {
				++mlog;
			}

			lbw_putbits(&lbw, mlog - (W_BITS - NUM_SLOTS), SLOT_BITS);

			if (mlog > W_BITS - NUM_SLOTS) {
				lbw_putbits(&lbw, offs - (1UL << mlog), mlog);
			}
			else {
				lbw_putbits(&lbw, offs, W_BITS - (NUM_SLOTS - 1));
			}
		}
	}

finalize:
	// Return compressed size
	return (unsigned long) (lbw_finalize(&lbw) - (unsigned char *) dst);
}

#endif /* CRUSH_LEPARSE_H_INCLUDED */
