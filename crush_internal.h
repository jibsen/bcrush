//
// bcrush - Example of CRUSH compression with BriefLZ algorithms
//
// Internal C/C++ header file
//
// Copyright (c) 2018 Joergen Ibsen
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

#ifndef CRUSH_INTERNAL_H_INCLUDED
#define CRUSH_INTERNAL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define W_BITS 21 // Window size (17..23)
#define W_SIZE (1UL << W_BITS)
#define W_MASK (W_SIZE - 1)
#define SLOT_BITS 4
#define NUM_SLOTS (1UL << SLOT_BITS)

#define A_BITS 2 // 1 xx
#define B_BITS 2 // 01 xx
#define C_BITS 2 // 001 xx
#define D_BITS 3 // 0001 xxx
#define E_BITS 5 // 00001 xxxxx
#define F_BITS 9 // 00000 xxxxxxxxx
#define A (1UL << A_BITS)
#define B ((1UL << B_BITS) + A)
#define C ((1UL << C_BITS) + B)
#define D ((1UL << D_BITS) + C)
#define E ((1UL << E_BITS) + D)
#define F ((1UL << F_BITS) + E)
#define MIN_MATCH 3
#define MAX_MATCH ((F - 1) + MIN_MATCH)

#define TOO_FAR (1UL << 16)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRUSH_INTERNAL_H_INCLUDED */
