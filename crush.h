/*
 * bcrush - Example of CRUSH compression with BriefLZ algorithms
 *
 * C/C++ header file
 *
 * Copyright (c) 2018-2020 Joergen Ibsen
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

#ifndef CRUSH_H_INCLUDED
#define CRUSH_H_INCLUDED

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRUSH_VER_MAJOR 0        /**< Major version number */
#define CRUSH_VER_MINOR 2        /**< Minor version number */
#define CRUSH_VER_PATCH 0        /**< Patch version number */
#define CRUSH_VER_STRING "0.2.0" /**< Version number as a string */

#ifdef CRUSH_DLL
#  if defined(_WIN32) || defined(__CYGWIN__)
#    ifdef CRUSH_DLL_EXPORTS
#      define CRUSH_API __declspec(dllexport)
#    else
#      define CRUSH_API __declspec(dllimport)
#    endif
#    define CRUSH_LOCAL
#  else
#    if __GNUC__ >= 4
#      define CRUSH_API __attribute__ ((visibility ("default")))
#      define CRUSH_LOCAL __attribute__ ((visibility ("hidden")))
#    else
#      define CRUSH_API
#      define CRUSH_LOCAL
#    endif
#  endif
#else
#  define CRUSH_API
#  define CRUSH_LOCAL
#endif

/**
 * Return value on error.
 */
#ifndef CRUSH_ERROR
#  define CRUSH_ERROR ((unsigned long) (-1))
#endif

/**
 * Get bound on compressed data size.
 *
 * @see crush_pack_level
 *
 * @param src_size number of bytes to compress
 * @return maximum size of compressed data
 */
CRUSH_API unsigned long
crush_max_packed_size(unsigned long src_size);

/**
 * Get required size of `workmem` buffer.
 *
 * @see crush_pack_level
 *
 * @param src_size number of bytes to compress
 * @param level compression level
 * @return required size in bytes of `workmem` buffer
 */
CRUSH_API unsigned long
crush_workmem_size_level(unsigned long src_size, int level);

/**
 * Compress `src_size` bytes of data from `src` to `dst`.
 *
 * Compression levels between 5 and 9 offer a trade-off between
 * time/space and ratio. Level 10 is optimal but very slow.
 *
 * @param src pointer to data
 * @param dst pointer to where to place compressed data
 * @param src_size number of bytes to compress
 * @param workmem pointer to memory for temporary use
 * @param level compression level
 * @return size of compressed data
 */
CRUSH_API unsigned long
crush_pack_level(const void *src, void *dst, unsigned long src_size,
                 void *workmem, int level);

/**
 * Decompress `depacked_size` bytes of data from `src` to `dst`.
 *
 * @param src pointer to compressed data
 * @param dst pointer to where to place decompressed data
 * @param depacked_size size of decompressed data
 * @return size of decompressed data
 */
CRUSH_API unsigned long
crush_depack(const void *src, void *dst, unsigned long depacked_size);

/**
 * Decompress `depacked_size` bytes of data from `src_file` to `dst`.
 *
 * @param src file containing compressed data
 * @param dst pointer to where to place decompressed data
 * @param depacked_size size of decompressed data
 * @return size of decompressed data
 */
CRUSH_API unsigned long
crush_depack_file(FILE *src_file, void *dst, unsigned long depacked_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRUSH_H_INCLUDED */
