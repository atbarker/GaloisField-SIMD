/*
	Copyright (c) 2015 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of CM256 nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CAUCHY_RS_H
#define CAUCHY_RS_H

/** \page GF GF(256) Math Module

    This module provides efficient implementations of bulk
    GF(2^^8) math operations over memory buffers.

    Addition is done over the base field in GF(2) meaning
    that addition is XOR between memory buffers.

    Multiplication is performed using table lookups via
    SIMD instructions.  This is somewhat slower than XOR,
    but fast enough to not become a major bottleneck when
    used sparingly.
*/

#include <linux/string.h>
#include <linux/types.h>
#include <asm/fpu/api.h>


//typedefs from intrinsic file, helps to define vectors
typedef long long __m128i __attribute__ ((__vector_size__ (16), __may_alias__));
typedef unsigned long long __v2du __attribute__ ((__vector_size__ (16)));
typedef long long __v2di __attribute__ ((__vector_size__ (16)));
typedef char __v16qi __attribute__ ((__vector_size__ (16)));

//AVX typedefs
typedef long long __m256i __attribute__ ((__vector_size__ (32), __may_alias__));
typedef unsigned long long __v4du __attribute__ ((__vector_size__ (32)));
typedef long long __v4di __attribute__ ((__vector_size__ (32)));
typedef char __v32qi __attribute__ ((__vector_size__ (32)));

//------------------------------------------------------------------------------
// Platform/Architecture

#if defined(ANDROID) || defined(IOS) || defined(LINUX_ARM)
    #define GF_TARGET_MOBILE
#endif // ANDROID

//this will likely not work
#if defined(__AVX2__)
    #define GF_TRY_AVX2 /* 256-bit */
    #include <immintrin.h>
    #define M256 __m256i
    #define GF_ALIGN_BYTES 32
#else // __AVX2__
    #define GF_ALIGN_BYTES 16
#endif // __AVX2__

//if we are not using a mobile version
#if !defined(GF_TARGET_MOBILE)
    //TODO: get rid of this in favor of inline assembly
    // Note: MSVC currently only supports SSSE3 but not AVX2
#endif // GF_TARGET_MOBILE

#if defined(HAVE_ARM_NEON_H)
    #include <arm_neon.h>
#endif // HAVE_ARM_NEON_H

#if defined(GF_TARGET_MOBILE)

    #define ALIGNED_ACCESSES /* Inputs must be aligned to GF_ALIGN_BYTES */

# if defined(HAVE_ARM_NEON_H)
    // Compiler-specific 128-bit SIMD register keyword
    #define M128 uint8x16_t
    #define GF_TRY_NEON
#else
    #define M128 uint64_t
# endif

#else // GF_TARGET_MOBILE

    // Compiler-specific 128-bit SIMD register keyword
    #define M128 __m128i

#endif // GF_TARGET_MOBILE

// Compiler-specific force inline keyword
#define FORCE_INLINE inline __attribute__((always_inline))

// Compiler-specific alignment keyword
// Note: Alignment only matters for ARM NEON where it should be 16
#define ALIGNED __attribute__((aligned(GF_ALIGN_BYTES)))

//------------------------------------------------------------------------------
// Portability

/// Swap two memory buffers in-place
void gf_memswap(void * __restrict vx, void * __restrict vy, int bytes);


//------------------------------------------------------------------------------
// GF(256) Context

/// The context object stores tables required to perform library calculations
typedef struct{
    /// We require memory to be aligned since the SIMD instructions benefit from
    /// or require aligned accesses to the table data.
    struct
    {
        ALIGNED M128 TABLE_LO_Y[256];
        ALIGNED M128 TABLE_HI_Y[256];
    } MM128;
#ifdef GF_TRY_AVX2
    struct
    {
        ALIGNED M256 TABLE_LO_Y[256];
        ALIGNED M256 TABLE_HI_Y[256];
    } MM256;
#endif // GF_TRY_AVX2

    /// Mul/Div/Inv/Sqr tables
    uint8_t GF_MUL_TABLE[256 * 256];
    uint8_t GF_DIV_TABLE[256 * 256];
    uint8_t GF_INV_TABLE[256];
    uint8_t GF_SQR_TABLE[256];

    /// Log/Exp tables
    uint16_t GF_LOG_TABLE[256];
    uint8_t GF_EXP_TABLE[512 * 2 + 1];

    /// Polynomial used
    unsigned Polynomial;
}gf_ctx;

//global context
//TODO get rid of the global context
extern gf_ctx GFContext;


//------------------------------------------------------------------------------
// Initialization

/**
    Initialize a context, filling in the tables.
    
    Thread-safety / Usage Notes:
    
    It is perfectly safe and encouraged to use a gf_ctx object from multiple
    threads.  The gf_init() is relatively expensive and should only be done
    once, though it will take less than a millisecond.
    
    The gf_ctx object must be aligned to 16 byte boundary.
    Simply tag the object with ALIGNED to achieve this.
    
    Example:
       static ALIGNED gf_ctx TheGFContext;
       gf_init(&TheGFContext, 0);
    
    Returns 0 on success and other values on failure.
*/
int gf_init(void);


//------------------------------------------------------------------------------
// Math Operations

/// return x + y
static FORCE_INLINE uint8_t gf_add(uint8_t x, uint8_t y)
{
    return (uint8_t)(x ^ y);
}

/// return x * y
/// For repeated multiplication by a constant, it is faster to put the constant in y.
static FORCE_INLINE uint8_t gf_mul(uint8_t x, uint8_t y)
{
    return GFContext.GF_MUL_TABLE[((unsigned)y << 8) + x];
}

/// return x / y
/// Memory-access optimized for constant divisors in y.
static FORCE_INLINE uint8_t gf_div(uint8_t x, uint8_t y)
{
    return GFContext.GF_DIV_TABLE[((unsigned)y << 8) + x];
}

/// return 1 / x
static FORCE_INLINE uint8_t gf_inv(uint8_t x)
{
    return GFContext.GF_INV_TABLE[x];
}

/// return x * x
static FORCE_INLINE uint8_t gf_sqr(uint8_t x)
{
    return GFContext.GF_SQR_TABLE[x];
}


//------------------------------------------------------------------------------
// Bulk Memory Math Operations

/// Performs "x[] += y[]" bulk memory XOR operation
void gf_add_mem(void * __restrict vx, const void * __restrict vy, int bytes);

/// Performs "z[] += x[] + y[]" bulk memory operation
void gf_add2_mem(void * __restrict vz, const void * __restrict vx, const void * __restrict vy, int bytes);

/// Performs "z[] = x[] + y[]" bulk memory operation
void gf_addset_mem(void * __restrict vz, const void * __restrict vx, const void * __restrict vy, int bytes);

/// Performs "z[] = x[] * y" bulk memory operation
void gf_mul_mem(void * __restrict vz, const void * __restrict vx, uint8_t y, int bytes);

/// Performs "z[] += x[] * y" bulk memory operation
void gf_muladd_mem(void * __restrict vz, uint8_t y, const void * __restrict vx, int bytes);

/// Performs "x[] /= y" bulk memory operation
static FORCE_INLINE void gf_div_mem(void * __restrict vz, const void * __restrict vx, uint8_t y, int bytes)
{
    // Multiply by inverse
    gf_mul_mem(vz, vx, y == 1 ? (uint8_t)1 : GFContext.GF_INV_TABLE[y], bytes);
}


//------------------------------------------------------------------------------
// Misc Operations

/// Swap two memory buffers in-place
void gf_memswap(void * __restrict vx, void * __restrict vy, int bytes);

/*
 * Verify binary compatibility with the API on startup.
 *
 * Example:
 * 	if (cm256_init()) exit(1);
 *
 * Returns 0 on success, and any other code indicates failure.
 */
int cm256_init(void);


// Encoder parameters
typedef struct cm256_encoder_params_t {
    // Original block count < 256
    int OriginalCount;

    // Recovery block count < 256
    int RecoveryCount;

    // Number of bytes per block (all blocks are the same size in bytes)
    int BlockBytes;
} cm256_encoder_params;

// Descriptor for data block
typedef struct cm256_block_t {
    // Pointer to data received.
    uint8_t* Block;

    // Block index.
    // For original data, it will be in the range
    //    [0..(originalCount-1)] inclusive.
    // For recovery data, the first one's Index must be originalCount,
    //    and it will be in the range
    //    [originalCount..(originalCount+recoveryCount-1)] inclusive.
    unsigned char Index;
    // Ignored during encoding, required during decoding.
} cm256_block;


// Compute the value to put in the Index member of cm256_block
static inline unsigned char cm256_get_recovery_block_index(cm256_encoder_params params, int recoveryBlockIndex)
{
    //assert(recoveryBlockIndex >= 0 && recoveryBlockIndex < params.RecoveryCount);
    return (unsigned char)(params.OriginalCount + recoveryBlockIndex);
}
static inline unsigned char cm256_get_original_block_index(cm256_encoder_params params, int originalBlockIndex)
{
    //assert(originalBlockIndex >= 0 && originalBlockIndex < params.OriginalCount);
    return (unsigned char)(originalBlockIndex);
}


/*
 * Cauchy MDS GF(256) encode
 *
 * This produces a set of recovery blocks that should be transmitted after the
 * original data blocks.
 *
 * It takes in 'originalCount' equal-sized blocks and produces 'recoveryCount'
 * equally-sized recovery blocks.
 *
 * The input 'originals' array allows more natural usage of the library.
 * The output recovery blocks are stored end-to-end in 'recoveryBlocks'.
 * 'recoveryBlocks' should have recoveryCount * blockBytes bytes available.
 *
 * Precondition: originalCount + recoveryCount <= 256
 *
 * When transmitting the data, the block index of the data should be sent,
 * and the recovery block index is also needed.  The decoder should also
 * be provided with the values of originalCount, recoveryCount and blockBytes.
 *
 * Example wire format:
 * [originalCount(1 byte)] [recoveryCount(1 byte)]
 * [blockIndex(1 byte)] [blockData(blockBytes bytes)]
 *
 * Be careful not to mix blocks from different encoders.
 *
 * It is possible to support variable-length data by including the original
 * data length at the front of each message in 2 bytes, such that when it is
 * recovered after a loss the data length is available in the block data and
 * the remaining bytes of padding can be neglected.
 *
 * Returns 0 on success, and any other code indicates failure.
 */
int cauchy_rs_encode(
    cm256_encoder_params params, // Encoder parameters
    cm256_block* originals,      // Array of pointers to original blocks
    void* recoveryBlocks);       // Output recovery blocks end-to-end

// Encode one block.
// Note: This function does not validate input, use with care.
void cauchy_rs_encode_block(
    cm256_encoder_params params, // Encoder parameters
    cm256_block* originals,      // Array of pointers to original blocks
    int recoveryBlockIndex,      // Return value from cm256_get_recovery_block_index()
    void* recoveryBlock);        // Output recovery block

/*
 * Cauchy MDS GF(256) decode
 *
 * This recovers the original data from the recovery data in the provided
 * blocks.  There should be 'originalCount' blocks in the provided array.
 * Recovery will always be possible if that many blocks are received.
 *
 * Provide the same values for 'originalCount', 'recoveryCount', and
 * 'blockBytes' used by the encoder.
 *
 * The block Index should be set to the block index of the original data,
 * as described in the cm256_block struct comments above.
 *
 * Recovery blocks will be replaced with original data and the Index
 * will be updated to indicate the original block that was recovered.
 *
 * Returns 0 on success, and any other code indicates failure.
 */
int cauchy_rs_decode(
    cm256_encoder_params params, // Encoder parameters
    cm256_block* blocks);        // Array of 'originalCount' blocks as described above


#endif // CAUCHY_RS_H
