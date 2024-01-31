/*
     This file is part of GNU libmicrohttpd
     Copyright (C) 2022-2023 Evgeny Grin (Karlson2k)

     GNU libmicrohttpd is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library.
     If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file microhttpd/sha512_256.c
 * @brief  Calculation of SHA-512/256 digest as defined in FIPS PUB 180-4 (2015)
 * @author Karlson2k (Evgeny Grin)
 */

#include "sha512_256.h"

#include <string.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include "mhd_bithelpers.h"
#include "mhd_assert.h"

/**
 * Initialise structure for SHA-512/256 calculation.
 *
 * @param ctx the calculation context
 */
void
MHD_SHA512_256_init (struct Sha512_256Ctx *ctx)
{
  /* Initial hash values, see FIPS PUB 180-4 clause 5.3.6.2 */
  /* Values generated by "IV Generation Function" as described in
   * clause 5.3.6 */
  ctx->H[0] = UINT64_C (0x22312194FC2BF72C);
  ctx->H[1] = UINT64_C (0x9F555FA3C84C64C2);
  ctx->H[2] = UINT64_C (0x2393B86B6F53B151);
  ctx->H[3] = UINT64_C (0x963877195940EABD);
  ctx->H[4] = UINT64_C (0x96283EE2A88EFFE3);
  ctx->H[5] = UINT64_C (0xBE5E1E2553863992);
  ctx->H[6] = UINT64_C (0x2B0199FC2C85B8AA);
  ctx->H[7] = UINT64_C (0x0EB72DDC81C52CA2);

  /* Initialise number of bytes and high part of number of bits. */
  ctx->count = 0;
  ctx->count_bits_hi = 0;
}


MHD_DATA_TRUNCATION_RUNTIME_CHECK_DISABLE_

/**
 * Base of SHA-512/256 transformation.
 * Gets full 128 bytes block of data and updates hash values;
 * @param H     hash values
 * @param data  the data buffer with #SHA512_256_BLOCK_SIZE bytes block
 */
static void
sha512_256_transform (uint64_t H[SHA512_256_HASH_SIZE_WORDS],
                      const void *data)
{
  /* Working variables,
     see FIPS PUB 180-4 clause 6.7, 6.4. */
  uint64_t a = H[0];
  uint64_t b = H[1];
  uint64_t c = H[2];
  uint64_t d = H[3];
  uint64_t e = H[4];
  uint64_t f = H[5];
  uint64_t g = H[6];
  uint64_t h = H[7];

  /* Data buffer, used as a cyclic buffer.
     See FIPS PUB 180-4 clause 5.2.2, 6.7, 6.4. */
  uint64_t W[16];

#ifndef _MHD_GET_64BIT_BE_ALLOW_UNALIGNED
  if (0 != (((uintptr_t) data) % _MHD_UINT64_ALIGN))
  { /* The input data is unaligned */
    /* Copy the unaligned input data to the aligned buffer */
    memcpy (W, data, sizeof(W));
    /* The W[] buffer itself will be used as the source of the data,
     * but the data will be reloaded in correct bytes order on
     * the next steps */
    data = (const void *) W;
  }
#endif /* _MHD_GET_64BIT_BE_ALLOW_UNALIGNED */

  /* 'Ch' and 'Maj' macro functions are defined with
     widely-used optimisation.
     See FIPS PUB 180-4 formulae 4.8, 4.9. */
#define Ch(x,y,z)     ( (z) ^ ((x) & ((y) ^ (z))) )
#define Maj(x,y,z)    ( ((x) & (y)) ^ ((z) & ((x) ^ (y))) )
  /* Unoptimized (original) versions: */
/* #define Ch(x,y,z)  ( ( (x) & (y) ) ^ ( ~(x) & (z) ) )          */
/* #define Maj(x,y,z) ( ((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)) ) */

  /* Four 'Sigma' macro functions.
     See FIPS PUB 180-4 formulae 4.10, 4.11, 4.12, 4.13. */
#define SIG0(x)  \
  ( _MHD_ROTR64 ((x), 28) ^ _MHD_ROTR64 ((x), 34) ^ _MHD_ROTR64 ((x), 39) )
#define SIG1(x)  \
  ( _MHD_ROTR64 ((x), 14) ^ _MHD_ROTR64 ((x), 18) ^ _MHD_ROTR64 ((x), 41) )
#define sig0(x)  \
  ( _MHD_ROTR64 ((x), 1) ^ _MHD_ROTR64 ((x), 8) ^ ((x) >> 7) )
#define sig1(x)  \
  ( _MHD_ROTR64 ((x), 19) ^ _MHD_ROTR64 ((x), 61) ^ ((x) >> 6) )

  /* One step of SHA-512/256 computation,
     see FIPS PUB 180-4 clause 6.4.2 step 3.
   * Note: this macro updates working variables in-place, without rotation.
   * Note: the first (vH += SIG1(vE) + Ch(vE,vF,vG) + kt + wt) equals T1 in
           FIPS PUB 180-4 clause 6.4.2 step 3.
           the second (vH += SIG0(vA) + Maj(vE,vF,vC) equals T1 + T2 in
           FIPS PUB 180-4 clause 6.4.2 step 3.
   * Note: 'wt' must be used exactly one time in this macro as it change other
           data as well every time when used. */
#define SHA2STEP64(vA,vB,vC,vD,vE,vF,vG,vH,kt,wt) do {                  \
    (vD) += ((vH) += SIG1 ((vE)) + Ch ((vE),(vF),(vG)) + (kt) + (wt));  \
    (vH) += SIG0 ((vA)) + Maj ((vA),(vB),(vC)); } while (0)

  /* Get value of W(t) from input data buffer for 0 <= t <= 15,
     See FIPS PUB 180-4 clause 6.2.
     Input data must be read in big-endian bytes order,
     see FIPS PUB 180-4 clause 3.1.2. */
#define GET_W_FROM_DATA(buf,t) \
  _MHD_GET_64BIT_BE (((const uint64_t*) (buf)) + (t))

  /* 'W' generation and assignment for 16 <= t <= 79.
     See FIPS PUB 180-4 clause 6.4.2.
     As only last 16 'W' are used in calculations, it is possible to
     use 16 elements array of W as a cyclic buffer.
   * Note: ((t-16) & 15) have same value as (t & 15) */
#define Wgen(w,t) ( (w)[(t - 16) & 15] + sig1 ((w)[((t) - 2) & 15])   \
                    + (w)[((t) - 7) & 15] + sig0 ((w)[((t) - 15) & 15]) )

#ifndef MHD_FAVOR_SMALL_CODE

  /* Note: instead of using K constants as array, all K values are specified
           individually for each step, see FIPS PUB 180-4 clause 4.2.3 for
           K values. */
  /* Note: instead of reassigning all working variables on each step,
           variables are rotated for each step:
             SHA2STEP64(a, b, c, d, e, f, g, h, K[0], data[0]);
             SHA2STEP64(h, a, b, c, d, e, f, g, K[1], data[1]);
           so current 'vD' will be used as 'vE' on next step,
           current 'vH' will be used as 'vA' on next step. */
#if _MHD_BYTE_ORDER == _MHD_BIG_ENDIAN
  if ((const void *) W == data)
  {
    /* The input data is already in the cyclic data buffer W[] in correct bytes
       order. */
    SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x428a2f98d728ae22), W[0]);
    SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x7137449123ef65cd), W[1]);
    SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0xb5c0fbcfec4d3b2f), W[2]);
    SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0xe9b5dba58189dbbc), W[3]);
    SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x3956c25bf348b538), W[4]);
    SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x59f111f1b605d019), W[5]);
    SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x923f82a4af194f9b), W[6]);
    SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0xab1c5ed5da6d8118), W[7]);
    SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0xd807aa98a3030242), W[8]);
    SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x12835b0145706fbe), W[9]);
    SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x243185be4ee4b28c), W[10]);
    SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x550c7dc3d5ffb4e2), W[11]);
    SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x72be5d74f27b896f), W[12]);
    SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x80deb1fe3b1696b1), W[13]);
    SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x9bdc06a725c71235), W[14]);
    SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0xc19bf174cf692694), W[15]);
  }
  else /* Combined with the next 'if' */
#endif /* _MHD_BYTE_ORDER == _MHD_BIG_ENDIAN */
  if (1)
  {
    /* During first 16 steps, before making any calculations on each step,
       the W element is read from the input data buffer as big-endian value and
       stored in the array of W elements. */
    SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x428a2f98d728ae22), \
                W[0] = GET_W_FROM_DATA (data, 0));
    SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x7137449123ef65cd), \
                W[1] = GET_W_FROM_DATA (data, 1));
    SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0xb5c0fbcfec4d3b2f), \
                W[2] = GET_W_FROM_DATA (data, 2));
    SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0xe9b5dba58189dbbc), \
                W[3] = GET_W_FROM_DATA (data, 3));
    SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x3956c25bf348b538), \
                W[4] = GET_W_FROM_DATA (data, 4));
    SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x59f111f1b605d019), \
                W[5] = GET_W_FROM_DATA (data, 5));
    SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x923f82a4af194f9b), \
                W[6] = GET_W_FROM_DATA (data, 6));
    SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0xab1c5ed5da6d8118), \
                W[7] = GET_W_FROM_DATA (data, 7));
    SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0xd807aa98a3030242), \
                W[8] = GET_W_FROM_DATA (data, 8));
    SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x12835b0145706fbe), \
                W[9] = GET_W_FROM_DATA (data, 9));
    SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x243185be4ee4b28c), \
                W[10] = GET_W_FROM_DATA (data, 10));
    SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x550c7dc3d5ffb4e2), \
                W[11] = GET_W_FROM_DATA (data, 11));
    SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x72be5d74f27b896f), \
                W[12] = GET_W_FROM_DATA (data, 12));
    SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x80deb1fe3b1696b1), \
                W[13] = GET_W_FROM_DATA (data, 13));
    SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x9bdc06a725c71235), \
                W[14] = GET_W_FROM_DATA (data, 14));
    SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0xc19bf174cf692694), \
                W[15] = GET_W_FROM_DATA (data, 15));
  }

  /* During last 64 steps, before making any calculations on each step,
     current W element is generated from other W elements of the cyclic buffer
     and the generated value is stored back in the cyclic buffer. */
  /* Note: instead of using K constants as array, all K values are specified
     individually for each step, see FIPS PUB 180-4 clause 4.2.3 for
     K values. */
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0xe49b69c19ef14ad2), \
              W[16 & 15] = Wgen (W,16));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0xefbe4786384f25e3), \
              W[17 & 15] = Wgen (W,17));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x0fc19dc68b8cd5b5), \
              W[18 & 15] = Wgen (W,18));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x240ca1cc77ac9c65), \
              W[19 & 15] = Wgen (W,19));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x2de92c6f592b0275), \
              W[20 & 15] = Wgen (W,20));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x4a7484aa6ea6e483), \
              W[21 & 15] = Wgen (W,21));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x5cb0a9dcbd41fbd4), \
              W[22 & 15] = Wgen (W,22));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x76f988da831153b5), \
              W[23 & 15] = Wgen (W,23));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x983e5152ee66dfab), \
              W[24 & 15] = Wgen (W,24));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0xa831c66d2db43210), \
              W[25 & 15] = Wgen (W,25));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0xb00327c898fb213f), \
              W[26 & 15] = Wgen (W,26));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0xbf597fc7beef0ee4), \
              W[27 & 15] = Wgen (W,27));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0xc6e00bf33da88fc2), \
              W[28 & 15] = Wgen (W,28));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0xd5a79147930aa725), \
              W[29 & 15] = Wgen (W,29));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x06ca6351e003826f), \
              W[30 & 15] = Wgen (W,30));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x142929670a0e6e70), \
              W[31 & 15] = Wgen (W,31));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x27b70a8546d22ffc), \
              W[32 & 15] = Wgen (W,32));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x2e1b21385c26c926), \
              W[33 & 15] = Wgen (W,33));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x4d2c6dfc5ac42aed), \
              W[34 & 15] = Wgen (W,34));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x53380d139d95b3df), \
              W[35 & 15] = Wgen (W,35));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x650a73548baf63de), \
              W[36 & 15] = Wgen (W,36));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x766a0abb3c77b2a8), \
              W[37 & 15] = Wgen (W,37));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x81c2c92e47edaee6), \
              W[38 & 15] = Wgen (W,38));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x92722c851482353b), \
              W[39 & 15] = Wgen (W,39));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0xa2bfe8a14cf10364), \
              W[40 & 15] = Wgen (W,40));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0xa81a664bbc423001), \
              W[41 & 15] = Wgen (W,41));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0xc24b8b70d0f89791), \
              W[42 & 15] = Wgen (W,42));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0xc76c51a30654be30), \
              W[43 & 15] = Wgen (W,43));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0xd192e819d6ef5218), \
              W[44 & 15] = Wgen (W,44));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0xd69906245565a910), \
              W[45 & 15] = Wgen (W,45));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0xf40e35855771202a), \
              W[46 & 15] = Wgen (W,46));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x106aa07032bbd1b8), \
              W[47 & 15] = Wgen (W,47));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x19a4c116b8d2d0c8), \
              W[48 & 15] = Wgen (W,48));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x1e376c085141ab53), \
              W[49 & 15] = Wgen (W,49));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x2748774cdf8eeb99), \
              W[50 & 15] = Wgen (W,50));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x34b0bcb5e19b48a8), \
              W[51 & 15] = Wgen (W,51));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x391c0cb3c5c95a63), \
              W[52 & 15] = Wgen (W,52));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x4ed8aa4ae3418acb), \
              W[53 & 15] = Wgen (W,53));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x5b9cca4f7763e373), \
              W[54 & 15] = Wgen (W,54));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x682e6ff3d6b2b8a3), \
              W[55 & 15] = Wgen (W,55));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x748f82ee5defb2fc), \
              W[56 & 15] = Wgen (W,56));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x78a5636f43172f60), \
              W[57 & 15] = Wgen (W,57));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x84c87814a1f0ab72), \
              W[58 & 15] = Wgen (W,58));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x8cc702081a6439ec), \
              W[59 & 15] = Wgen (W,59));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x90befffa23631e28), \
              W[60 & 15] = Wgen (W,60));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0xa4506cebde82bde9), \
              W[61 & 15] = Wgen (W,61));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0xbef9a3f7b2c67915), \
              W[62 & 15] = Wgen (W,62));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0xc67178f2e372532b), \
              W[63 & 15] = Wgen (W,63));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0xca273eceea26619c), \
              W[64 & 15] = Wgen (W,64));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0xd186b8c721c0c207), \
              W[65 & 15] = Wgen (W,65));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0xeada7dd6cde0eb1e), \
              W[66 & 15] = Wgen (W,66));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0xf57d4f7fee6ed178), \
              W[67 & 15] = Wgen (W,67));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x06f067aa72176fba), \
              W[68 & 15] = Wgen (W,68));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x0a637dc5a2c898a6), \
              W[69 & 15] = Wgen (W,69));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x113f9804bef90dae), \
              W[70 & 15] = Wgen (W,70));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x1b710b35131c471b), \
              W[71 & 15] = Wgen (W,71));
  SHA2STEP64 (a, b, c, d, e, f, g, h, UINT64_C (0x28db77f523047d84), \
              W[72 & 15] = Wgen (W,72));
  SHA2STEP64 (h, a, b, c, d, e, f, g, UINT64_C (0x32caab7b40c72493), \
              W[73 & 15] = Wgen (W,73));
  SHA2STEP64 (g, h, a, b, c, d, e, f, UINT64_C (0x3c9ebe0a15c9bebc), \
              W[74 & 15] = Wgen (W,74));
  SHA2STEP64 (f, g, h, a, b, c, d, e, UINT64_C (0x431d67c49c100d4c), \
              W[75 & 15] = Wgen (W,75));
  SHA2STEP64 (e, f, g, h, a, b, c, d, UINT64_C (0x4cc5d4becb3e42b6), \
              W[76 & 15] = Wgen (W,76));
  SHA2STEP64 (d, e, f, g, h, a, b, c, UINT64_C (0x597f299cfc657e2a), \
              W[77 & 15] = Wgen (W,77));
  SHA2STEP64 (c, d, e, f, g, h, a, b, UINT64_C (0x5fcb6fab3ad6faec), \
              W[78 & 15] = Wgen (W,78));
  SHA2STEP64 (b, c, d, e, f, g, h, a, UINT64_C (0x6c44198c4a475817), \
              W[79 & 15] = Wgen (W,79));
#else  /* MHD_FAVOR_SMALL_CODE */
  if (1)
  {
    unsigned int t;
    /* K constants array.
       See FIPS PUB 180-4 clause 4.2.3 for K values. */
    static const uint64_t K[80] =
    { UINT64_C (0x428a2f98d728ae22), UINT64_C (0x7137449123ef65cd),
      UINT64_C (0xb5c0fbcfec4d3b2f), UINT64_C (0xe9b5dba58189dbbc),
      UINT64_C (0x3956c25bf348b538), UINT64_C (0x59f111f1b605d019),
      UINT64_C (0x923f82a4af194f9b), UINT64_C (0xab1c5ed5da6d8118),
      UINT64_C (0xd807aa98a3030242), UINT64_C (0x12835b0145706fbe),
      UINT64_C (0x243185be4ee4b28c), UINT64_C (0x550c7dc3d5ffb4e2),
      UINT64_C (0x72be5d74f27b896f), UINT64_C (0x80deb1fe3b1696b1),
      UINT64_C (0x9bdc06a725c71235), UINT64_C (0xc19bf174cf692694),
      UINT64_C (0xe49b69c19ef14ad2), UINT64_C (0xefbe4786384f25e3),
      UINT64_C (0x0fc19dc68b8cd5b5), UINT64_C (0x240ca1cc77ac9c65),
      UINT64_C (0x2de92c6f592b0275), UINT64_C (0x4a7484aa6ea6e483),
      UINT64_C (0x5cb0a9dcbd41fbd4), UINT64_C (0x76f988da831153b5),
      UINT64_C (0x983e5152ee66dfab), UINT64_C (0xa831c66d2db43210),
      UINT64_C (0xb00327c898fb213f), UINT64_C (0xbf597fc7beef0ee4),
      UINT64_C (0xc6e00bf33da88fc2), UINT64_C (0xd5a79147930aa725),
      UINT64_C (0x06ca6351e003826f), UINT64_C (0x142929670a0e6e70),
      UINT64_C (0x27b70a8546d22ffc), UINT64_C (0x2e1b21385c26c926),
      UINT64_C (0x4d2c6dfc5ac42aed), UINT64_C (0x53380d139d95b3df),
      UINT64_C (0x650a73548baf63de), UINT64_C (0x766a0abb3c77b2a8),
      UINT64_C (0x81c2c92e47edaee6), UINT64_C (0x92722c851482353b),
      UINT64_C (0xa2bfe8a14cf10364), UINT64_C (0xa81a664bbc423001),
      UINT64_C (0xc24b8b70d0f89791), UINT64_C (0xc76c51a30654be30),
      UINT64_C (0xd192e819d6ef5218), UINT64_C (0xd69906245565a910),
      UINT64_C (0xf40e35855771202a), UINT64_C (0x106aa07032bbd1b8),
      UINT64_C (0x19a4c116b8d2d0c8), UINT64_C (0x1e376c085141ab53),
      UINT64_C (0x2748774cdf8eeb99), UINT64_C (0x34b0bcb5e19b48a8),
      UINT64_C (0x391c0cb3c5c95a63), UINT64_C (0x4ed8aa4ae3418acb),
      UINT64_C (0x5b9cca4f7763e373), UINT64_C (0x682e6ff3d6b2b8a3),
      UINT64_C (0x748f82ee5defb2fc), UINT64_C (0x78a5636f43172f60),
      UINT64_C (0x84c87814a1f0ab72), UINT64_C (0x8cc702081a6439ec),
      UINT64_C (0x90befffa23631e28), UINT64_C (0xa4506cebde82bde9),
      UINT64_C (0xbef9a3f7b2c67915), UINT64_C (0xc67178f2e372532b),
      UINT64_C (0xca273eceea26619c), UINT64_C (0xd186b8c721c0c207),
      UINT64_C (0xeada7dd6cde0eb1e), UINT64_C (0xf57d4f7fee6ed178),
      UINT64_C (0x06f067aa72176fba), UINT64_C (0x0a637dc5a2c898a6),
      UINT64_C (0x113f9804bef90dae), UINT64_C (0x1b710b35131c471b),
      UINT64_C (0x28db77f523047d84), UINT64_C (0x32caab7b40c72493),
      UINT64_C (0x3c9ebe0a15c9bebc), UINT64_C (0x431d67c49c100d4c),
      UINT64_C (0x4cc5d4becb3e42b6), UINT64_C (0x597f299cfc657e2a),
      UINT64_C (0x5fcb6fab3ad6faec), UINT64_C (0x6c44198c4a475817)};

    /* One step of SHA-512/256 computation with working variables rotation,
       see FIPS PUB 180-4 clause 6.4.2 step 3.
     * Note: this version of macro reassign all working variable on
             each step. */
#define SHA2STEP64RV(vA,vB,vC,vD,vE,vF,vG,vH,kt,wt) do {              \
  uint64_t tmp_h_ = (vH);                                             \
  SHA2STEP64((vA),(vB),(vC),(vD),(vE),(vF),(vG),tmp_h_,(kt),(wt));    \
  (vH) = (vG);                                                        \
  (vG) = (vF);                                                        \
  (vF) = (vE);                                                        \
  (vE) = (vD);                                                        \
  (vD) = (vC);                                                        \
  (vC) = (vB);                                                        \
  (vB) = (vA);                                                        \
  (vA) = tmp_h_;  } while (0)

    /* During first 16 steps, before making any calculations on each step,
       the W element is read from the input data buffer as big-endian value and
       stored in the array of W elements. */
    for (t = 0; t < 16; ++t)
    {
      SHA2STEP64RV (a, b, c, d, e, f, g, h, K[t], \
                    W[t] = GET_W_FROM_DATA (data, t));
    }
    /* During last 64 steps, before making any calculations on each step,
       current W element is generated from other W elements of the cyclic buffer
       and the generated value is stored back in the cyclic buffer. */
    for (t = 16; t < 80; ++t)
    {
      SHA2STEP64RV (a, b, c, d, e, f, g, h, K[t], \
                    W[t & 15] = Wgen (W,t));
    }
  }
#endif /* MHD_FAVOR_SMALL_CODE */

  /* Compute and store the intermediate hash.
     See FIPS PUB 180-4 clause 6.4.2 step 4. */
  H[0] += a;
  H[1] += b;
  H[2] += c;
  H[3] += d;
  H[4] += e;
  H[5] += f;
  H[6] += g;
  H[7] += h;
}


/**
 * Process portion of bytes.
 *
 * @param ctx the calculation context
 * @param data bytes to add to hash
 * @param length number of bytes in @a data
 */
void
MHD_SHA512_256_update (struct Sha512_256Ctx *ctx,
                       const uint8_t *data,
                       size_t length)
{
  unsigned int bytes_have; /**< Number of bytes in the context buffer */
  uint64_t count_hi; /**< The high part to be moved to another variable */

  mhd_assert ((data != NULL) || (length == 0));

#ifndef MHD_FAVOR_SMALL_CODE
  if (0 == length)
    return; /* Shortcut, do nothing */
#endif /* ! MHD_FAVOR_SMALL_CODE */

  /* Note: (count & (SHA512_256_BLOCK_SIZE-1))
           equals (count % SHA512_256_BLOCK_SIZE) for this block size. */
  bytes_have = (unsigned int) (ctx->count & (SHA512_256_BLOCK_SIZE - 1));
  ctx->count += length;
  count_hi = ctx->count >> 61;
  if (0 != count_hi)
  {
    ctx->count_bits_hi += count_hi;
    ctx->count &= UINT64_C (0x1FFFFFFFFFFFFFFF);
  }

  if (0 != bytes_have)
  {
    unsigned int bytes_left = SHA512_256_BLOCK_SIZE - bytes_have;
    if (length >= bytes_left)
    {     /* Combine new data with data in the buffer and
             process the full block. */
      memcpy (((uint8_t *) ctx->buffer) + bytes_have,
              data,
              bytes_left);
      data += bytes_left;
      length -= bytes_left;
      sha512_256_transform (ctx->H, ctx->buffer);
      bytes_have = 0;
    }
  }

  while (SHA512_256_BLOCK_SIZE <= length)
  {   /* Process any full blocks of new data directly,
         without copying to the buffer. */
    sha512_256_transform (ctx->H, data);
    data += SHA512_256_BLOCK_SIZE;
    length -= SHA512_256_BLOCK_SIZE;
  }

  if (0 != length)
  {   /* Copy incomplete block of new data (if any)
         to the buffer. */
    memcpy (((uint8_t *) ctx->buffer) + bytes_have, data, length);
  }
}


/**
 * Size of "length" insertion in bits.
 * See FIPS PUB 180-4 clause 5.1.2.
 */
#define SHA512_256_SIZE_OF_LEN_ADD_BITS 128

/**
 * Size of "length" insertion in bytes.
 */
#define SHA512_256_SIZE_OF_LEN_ADD (SHA512_256_SIZE_OF_LEN_ADD_BITS / 8)

/**
 * Finalise SHA-512/256 calculation, return digest.
 *
 * @param ctx the calculation context
 * @param[out] digest set to the hash, must be #SHA512_256_DIGEST_SIZE bytes
 */
void
MHD_SHA512_256_finish (struct Sha512_256Ctx *ctx,
                       uint8_t digest[SHA512_256_DIGEST_SIZE])
{
  uint64_t num_bits;   /**< Number of processed bits */
  unsigned int bytes_have; /**< Number of bytes in the context buffer */

  /* Memorise the number of processed bits.
     The padding and other data added here during the postprocessing must
     not change the amount of hashed data. */
  num_bits = ctx->count << 3;

  /* Note: (count & (SHA512_256_BLOCK_SIZE-1))
           equals (count % SHA512_256_BLOCK_SIZE) for this block size. */
  bytes_have = (unsigned int) (ctx->count & (SHA512_256_BLOCK_SIZE - 1));

  /* Input data must be padded with a single bit "1", then with zeros and
     the finally the length of data in bits must be added as the final bytes
     of the last block.
     See FIPS PUB 180-4 clause 5.1.2. */

  /* Data is always processed in form of bytes (not by individual bits),
     therefore position of the first padding bit in byte is always
     predefined (0x80). */
  /* Buffer always have space for one byte at least (as full buffers are
     processed immediately). */
  ((uint8_t *) ctx->buffer)[bytes_have++] = 0x80;

  if (SHA512_256_BLOCK_SIZE - bytes_have < SHA512_256_SIZE_OF_LEN_ADD)
  {   /* No space in the current block to put the total length of message.
         Pad the current block with zeros and process it. */
    if (bytes_have < SHA512_256_BLOCK_SIZE)
      memset (((uint8_t *) ctx->buffer) + bytes_have, 0,
              SHA512_256_BLOCK_SIZE - bytes_have);
    /* Process the full block. */
    sha512_256_transform (ctx->H, ctx->buffer);
    /* Start the new block. */
    bytes_have = 0;
  }

  /* Pad the rest of the buffer with zeros. */
  memset (((uint8_t *) ctx->buffer) + bytes_have, 0,
          SHA512_256_BLOCK_SIZE - SHA512_256_SIZE_OF_LEN_ADD - bytes_have);
  /* Put high part of number of bits in processed message and then lower
     part of number of bits as big-endian values.
     See FIPS PUB 180-4 clause 5.1.2. */
  /* Note: the target location is predefined and buffer is always aligned */
  _MHD_PUT_64BIT_BE (ctx->buffer + SHA512_256_BLOCK_SIZE_WORDS - 2,
                     ctx->count_bits_hi);
  _MHD_PUT_64BIT_BE (ctx->buffer + SHA512_256_BLOCK_SIZE_WORDS - 1,
                     num_bits);
  /* Process the full final block. */
  sha512_256_transform (ctx->H, ctx->buffer);

  /* Put in BE mode the leftmost part of the hash as the final digest.
     See FIPS PUB 180-4 clause 6.7. */
#ifndef _MHD_PUT_64BIT_BE_UNALIGNED
  if (1
#ifndef MHD_FAVOR_SMALL_CODE
      && (0 != ((uintptr_t) digest) % _MHD_UINT64_ALIGN)
#endif /* MHD_FAVOR_SMALL_CODE */
      )
  {
    /* If storing of the final result requires aligned address and
       the destination address is not aligned or compact code is used,
       store the final digest in aligned temporary buffer first, then
       copy it to the destination. */
    uint64_t alig_dgst[SHA512_256_DIGEST_SIZE_WORDS];
    _MHD_PUT_64BIT_BE (alig_dgst + 0, ctx->H[0]);
    _MHD_PUT_64BIT_BE (alig_dgst + 1, ctx->H[1]);
    _MHD_PUT_64BIT_BE (alig_dgst + 2, ctx->H[2]);
    _MHD_PUT_64BIT_BE (alig_dgst + 3, ctx->H[3]);
    /* Copy result to the unaligned destination address */
    memcpy (digest, alig_dgst, SHA512_256_DIGEST_SIZE);
  }
#ifndef MHD_FAVOR_SMALL_CODE
  else /* Combined with the next 'if' */
#endif /* MHD_FAVOR_SMALL_CODE */
#endif /* ! _MHD_PUT_64BIT_BE_UNALIGNED */
#if ! defined(MHD_FAVOR_SMALL_CODE) || defined(_MHD_PUT_64BIT_BE_UNALIGNED)
  if (1)
  {
    /* Use cast to (void*) here to mute compiler alignment warnings.
     * Compilers are not smart enough to see that alignment has been checked. */
    _MHD_PUT_64BIT_BE ((void *) (digest + 0 * SHA512_256_BYTES_IN_WORD), \
                       ctx->H[0]);
    _MHD_PUT_64BIT_BE ((void *) (digest + 1 * SHA512_256_BYTES_IN_WORD), \
                       ctx->H[1]);
    _MHD_PUT_64BIT_BE ((void *) (digest + 2 * SHA512_256_BYTES_IN_WORD), \
                       ctx->H[2]);
    _MHD_PUT_64BIT_BE ((void *) (digest + 3 * SHA512_256_BYTES_IN_WORD), \
                       ctx->H[3]);
  }
#endif /* ! MHD_FAVOR_SMALL_CODE || _MHD_PUT_64BIT_BE_UNALIGNED */

  /* Erase potentially sensitive data. */
  memset (ctx, 0, sizeof(struct Sha512_256Ctx));
}


MHD_DATA_TRUNCATION_RUNTIME_CHECK_RESTORE_
