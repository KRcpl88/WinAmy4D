/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

#include "amy.h"

#include "types.h"
#include <stddef.h>

/**
   A C-program for MT19937-64 (2004/9/29 version).
   Coded by Takuji Nishimura and Makoto Matsumoto.

   This is a 64-bit version of Mersenne Twister pseudorandom number
   generator.

   Before using, initialize the state by using init_genrand64(seed)
   or init_by_array64(init_key, key_length).

   Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   References:
   T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
     ACM Transactions on Modeling and
     Computer Simulation 10. (2000) 348--357.
   M. Matsumoto and T. Nishimura,
     ``Mersenne Twister: a 623-dimensionally equidistributed
       uniform pseudorandom number generator''
     ACM Transactions on Modeling and
     Computer Simulation 8. (Jan. 1998) 3--30.

   Any feedback is very welcome.
   http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
 */

#define NN 312
#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL         /* Least significant 31 bits */

struct mt19937_64 {
    unsigned long long mt[NN];
    size_t mti;
};

static struct mt19937_64 context;

/* initializes mt[NN] with a seed */
void init_genrand64(unsigned long long qwSeed) {
    context.mt[0] = qwSeed;
    for (context.mti = 1; context.mti < NN; context.mti++)
        context.mt[context.mti] =
            (6364136223846793005ULL * (context.mt[context.mti - 1] ^
                                       (context.mt[context.mti - 1] >> 62)) +
             context.mti);
}

ran_t Random64(void) {
    /* This is the altered Cocoa with Love implementation. */
    size_t i;
    size_t j;
    unsigned long long qwResult;

    if (context.mti >= NN) { /* generate NN words at one time */
        size_t qwMid = NN / 2;
        unsigned long long qwStateMid = context.mt[qwMid];
        unsigned long long qwX;
        unsigned long long qwY;

        /* NOTE: this "untwist" code is modified from the original to improve
         * performance, as described here:
         * http://www.cocoawithlove.com/blog/2016/05/19/random-numbers.html
         * These modifications are offered for use under the original icense at
         * the top of this file.
         */
        for (i = 0, j = qwMid; i != qwMid - 1; i++, j++) {
            qwX = (context.mt[i] & UM) | (context.mt[i + 1] & LM);
            context.mt[i] = context.mt[i + qwMid] ^ (qwX >> 1) ^
                            ((context.mt[i + 1] & 1) * MATRIX_A);
            qwY = (context.mt[j] & UM) | (context.mt[j + 1] & LM);
            context.mt[j] = context.mt[j - qwMid] ^ (qwY >> 1) ^
                            ((context.mt[j + 1] & 1) * MATRIX_A);
        }
        qwX = (context.mt[qwMid - 1] & UM) | (qwStateMid & LM);
        context.mt[qwMid - 1] =
            context.mt[NN - 1] ^ (qwX >> 1) ^ ((qwStateMid & 1) * MATRIX_A);
        qwY = (context.mt[NN - 1] & UM) | (context.mt[0] & LM);
        context.mt[NN - 1] =
            context.mt[qwMid - 1] ^ (qwY >> 1) ^ ((context.mt[0] & 1) * MATRIX_A);

        context.mti = 0;
    }

    qwResult = context.mt[context.mti];
    context.mti = context.mti + 1;

    qwResult ^= (qwResult >> 29) & 0x5555555555555555ULL;
    qwResult ^= (qwResult << 17) & 0x71D67FFFEDA60000ULL;
    qwResult ^= (qwResult << 37) & 0xFFF7EEE000000000ULL;
    qwResult ^= (qwResult >> 43);

    return qwResult;
}

double Random(void) { return (Random64() >> 11) * (1.0 / 9007199254740991.0); }

void InitRandom(ran_t qwSeed) { init_genrand64(qwSeed); }