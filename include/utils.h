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

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

#define ONE_SECOND 100u

extern uint16_t g_nVerbosity;
extern uint16_t g_nDebugMode;

void OpenLogFile(const char *name);
void Print(int, const char *, ...);
void PrintDebug(int, const char *, ...);

/*
 * AMY_ASSERT - diagnostic assertion for "this should never happen" scenarios.
 *
 * When the condition is false it (1) logs a descriptive message via Print so the
 * failure is visible in the log file (e.g. when running under WinAmyGUI which
 * has no console) and (2) traps into the debugger via the standard assert()
 * macro so the offending state can be inspected. In release builds (NDEBUG
 * defined) it expands to nothing, exactly like the standard assert(), so it adds
 * no overhead on the search hot path.
 *
 * Usage: AMY_ASSERT(cond, fmt, ...) - a printf-style format string is required.
 */
#ifdef NDEBUG
#define AMY_ASSERT(cond, ...) ((void)0)
#else
#include <assert.h>
#define AMY_ASSERT(cond, ...)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            Print(0, "ASSERTION FAILED (%s:%d): ", __FILE__, __LINE__);        \
            Print(0, __VA_ARGS__);                                             \
            assert(cond);                                                      \
        }                                                                      \
    } while (0)
#endif

int InputReady(void);
int ReadLine(char *buffer, int cnt);
char *FormatTime(unsigned long, char *, size_t);
char *FormatScore(int, char *, size_t);
char *FormatCount(unsigned long, char *, size_t);
unsigned long GetTime(void);
void GetTmpFileName(char *, size_t);
char *nextToken(char **, const char *);
int Percentage(unsigned long, unsigned long);
char *strip(char *);

#endif
