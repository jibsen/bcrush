//
// bcrush - Example of CRUSH compression with BriefLZ algorithms
//
// Forwards dynamic programming parse using binary trees
//
// Copyright (c) 2020 Joergen Ibsen
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

#ifndef CRUSH_BTPARSE_H_INCLUDED
#define CRUSH_BTPARSE_H_INCLUDED

static size_t
crush_btparse_workmem_size(size_t src_size)
{
	return (5 * src_size + 3 + LOOKUP_SIZE) * sizeof(uint32_t);
}

// Forwards dynamic programming parse using binary trees, checking all
// possible matches.
//
// The match search uses a binary tree for each hash entry, which is updated
// dynamically as it is searched by re-rooting the tree at the search string.
//
// This does not result in balanced trees on all inputs, but often works well
// in practice, and has the advantage that we get the matches in order from
// closest and back.
//
// A drawback is the memory requirement of 5 * src_size words, since we cannot
// overlap the arrays in a forwards parse.
//
// This match search method is found in LZMA by Igor Pavlov, libdeflate
// by Eric Biggers, and other libraries.
//
static unsigned long
crush_pack_btparse(const void *src, void *dst, unsigned long src_size, void *workmem,
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

	uint32_t *const cost = (uint32_t *) workmem;
	uint32_t *const mpos = cost + src_size + 1;
	uint32_t *const mlen = mpos + src_size + 1;
	uint32_t *const nodes = mlen + src_size + 1;
	uint32_t *const lookup = nodes + 2 * src_size;

	// Initialize lookup
	for (unsigned long i = 0; i < LOOKUP_SIZE; ++i) {
		lookup[i] = NO_MATCH_POS;
	}

	// Initialize to all literals with infinite cost
	for (unsigned long i = 0; i <= src_size; ++i) {
		cost[i] = UINT32_MAX;
		mlen[i] = 1;
	}

	cost[0] = 0;

	// Next position where we are going to check matches
	//
	// This is used to skip matching while still updating the trees when
	// we find a match that is accept_len or longer.
	//
	unsigned long next_match_cur = 0;

	// Phase 1: Find lowest cost path arriving at each position
	for (unsigned long cur = 0; cur <= last_match_pos; ++cur) {
		// Check literal
		if (cost[cur + 1] > cost[cur] + 9) {
			cost[cur + 1] = cost[cur] + 9;
			mlen[cur + 1] = 1;
		}

		if (cur > next_match_cur) {
			next_match_cur = cur;
		}

		unsigned long max_len = MIN_MATCH - 1;

		// Look up first match for current position
		//
		// pos is the current root of the tree of strings with this
		// hash. We are going to re-root the tree so cur becomes the
		// new root.
		//
		const unsigned long hash = crush_hash3_bits(&in[cur], CRUSH_HASH_BITS);
		unsigned long pos = lookup[hash];
		lookup[hash] = cur;

		uint32_t *lt_node = &nodes[2 * cur];
		uint32_t *gt_node = &nodes[2 * cur + 1];
		unsigned long lt_len = 0;
		unsigned long gt_len = 0;

		assert(pos == NO_MATCH_POS || pos < cur);

		// If we are checking matches, allow lengths up to MAX_MATCH,
		// otherwise compare only up to accept_len
		const unsigned long len_left = src_size - cur > MAX_MATCH ? MAX_MATCH : src_size - cur;
		const unsigned long len_limit = cur == next_match_cur ? len_left
		                              : accept_len < len_left ? accept_len
		                              : len_left;
		unsigned long num_chain = max_depth;

		// Check matches
		for (;;) {
			// If at bottom of tree, mark leaf nodes
			//
			// In case we reached max_depth, this also prunes the
			// subtree we have not searched yet and do not know
			// where belongs.
			//
			if (pos == NO_MATCH_POS || cur - pos > W_SIZE || num_chain-- == 0) {
				*lt_node = NO_MATCH_POS;
				*gt_node = NO_MATCH_POS;

				break;
			}

			// The string at pos is lexicographically greater than
			// a string that matched in the first lt_len positions,
			// and less than a string that matched in the first
			// gt_len positions, so it must match up to at least
			// the minimum of these.
			unsigned long len = lt_len < gt_len ? lt_len : gt_len;

			// Find match len
			while (len < len_limit && in[pos + len] == in[cur + len]) {
				++len;
			}

			// Extend current match if possible
			//
			// Note that we are checking matches in order from the
			// closest and back. This means for a match further
			// away, the encoding of all lengths up to the current
			// max length will always be longer or equal, so we need
			// only consider the extension.
			//
			if (cur == next_match_cur && len > max_len) {
				for (unsigned long i = max_len + 1; i <= len; ++i) {
					unsigned long match_cost = crush_match_cost(cur - pos - 1, i);

					assert(match_cost < UINT32_MAX - cost[cur]);

					unsigned long cost_there = cost[cur] + match_cost;

					if (cost_there < cost[cur + i]) {
						cost[cur + i] = cost_there;
						mpos[cur + i] = cur - pos - 1;
						mlen[cur + i] = i;
					}
				}

				max_len = len;

				if (len >= accept_len) {
					next_match_cur = cur + len;
				}
			}

			// If we reach maximum match length, the string at pos
			// is equal to cur, so we can assign the left and right
			// subtrees.
			//
			// This removes pos from the tree, but we added cur
			// which is equal and closer for future matches.
			//
			if (len >= accept_len || len == len_limit) {
				*lt_node = nodes[2 * pos];
				*gt_node = nodes[2 * pos + 1];

				break;
			}

			// Go to previous match and restructure tree
			//
			// lt_node points to a node that is going to contain
			// elements lexicographically less than cur (the search
			// string).
			//
			// If the string at pos is less than cur, we set that
			// lt_node to pos. We know that all elements in the
			// left subtree are less than pos, and thus less than
			// cur, so we point lt_node at the right subtree of
			// pos and continue our search there.
			//
			// The equivalent applies to gt_node when the string at
			// pos is greater than cur.
			//
			if (in[pos + len] < in[cur + len]) {
				*lt_node = pos;
				lt_node = &nodes[2 * pos + 1];
				assert(*lt_node == NO_MATCH_POS || *lt_node < pos);
				pos = *lt_node;
				lt_len = len;
			}
			else {
				*gt_node = pos;
				gt_node = &nodes[2 * pos];
				assert(*gt_node == NO_MATCH_POS || *gt_node < pos);
				pos = *gt_node;
				gt_len = len;
			}
		}
	}

	for (unsigned long cur = last_match_pos + 1; cur < src_size; ++cur) {
		// Check literal
		if (cost[cur + 1] > cost[cur] + 9) {
			cost[cur + 1] = cost[cur] + 9;
			mlen[cur + 1] = 1;
		}
	}

	// Phase 2: Follow lowest cost path backwards gathering tokens
	unsigned long next_token = src_size;

	for (unsigned long cur = src_size; cur > 0; cur -= mlen[cur], --next_token) {
		mlen[next_token] = mlen[cur];
		mpos[next_token] = mpos[cur];
	}

	// Phase 3: Output tokens
	unsigned long cur = 0;
	for (unsigned long i = next_token + 1; i <= src_size; cur += mlen[i++]) {
		if (mlen[i] == 1) {
			lbw_putbits(&lbw, (uint32_t) in[cur] << 1, 9);
		}
		else {
			const unsigned long offs = mpos[i];

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

			if (offs >= (2UL << (W_BITS - NUM_SLOTS))) {
				unsigned long mlog = crush_log2(offs);

				lbw_putbits(&lbw, mlog - (W_BITS - NUM_SLOTS), SLOT_BITS);
				lbw_putbits(&lbw, offs - (1UL << mlog), mlog);
			}
			else {
				lbw_putbits(&lbw, 0, SLOT_BITS);
				lbw_putbits(&lbw, offs, W_BITS - (NUM_SLOTS - 1));
			}
		}
	}

finalize:
	// Return compressed size
	return (unsigned long) (lbw_finalize(&lbw) - (unsigned char *) dst);
}

#endif /* CRUSH_BTPARSE_H_INCLUDED */
