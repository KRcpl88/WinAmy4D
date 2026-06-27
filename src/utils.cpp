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

/*
 * utils.c - utility routines
 */

#include "amy.h"

#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#include "search.h"
#include "utils.h"

extern bool XBoardMode;

FILE *LogFile = NULL;
uint16_t g_nVerbosity = 9;

/*
 * When g_nDebugMode is non-zero (enabled via the -debug command line option),
 * PrintDebug also writes its output to the log file (if one is open).
 */
uint16_t g_nDebugMode = 0;

/**
 * Open a log file, remember fp in global variable LogFile
 */
void OpenLogFile(const char *pszName) {
    if (LogFile) {
        fclose(LogFile);
    }
    LogFile = fopen(pszName, "w");
}

/**
 * Print something to stdout and to the logfile.
 */
void CDECL Print(int nVb, const char *pszFmt, ...) {
    if (nVb < g_nVerbosity) {
        va_list pVa;
        va_start(pVa, pszFmt);
        vprintf(pszFmt, pVa);
        fflush(stdout);
        va_end(pVa);
    }
    if (LogFile) {
        va_list pVa;
        va_start(pVa, pszFmt);
        vfprintf(LogFile, pszFmt, pVa);
        fflush(LogFile);
        va_end(pVa);
    }
}

/**
 * Print to stdout, and additionally mirror to the log file when debug mode is
 * enabled (via the -debug command line option) and a log file is open.
 *
 * Historically this wrote to stdout only. However, WinAmyGUI is a
 * /SUBSYSTEM:WINDOWS application with no attached console, so anything written
 * to stdout here is discarded and cannot be captured in GUI mode. To make these
 * diagnostics (e.g. search progress / xboard "thinking" lines, attack-map
 * maintenance) available when running under the GUI, we also append to the log
 * file when g_nDebugMode is set, unconditionally of the verbosity level. The stdout
 * copy remains gated by g_nVerbosity for the console front-ends.
 */
void CDECL PrintDebug(int nVb, const char *pszFmt, ...) {
    if (nVb < g_nVerbosity) {
        va_list pVa;
        va_start(pVa, pszFmt);
        vprintf(pszFmt, pVa);
        fflush(stdout);
        va_end(pVa);
    }

    if (g_nDebugMode && LogFile) {
        va_list pVa;
        va_start(pVa, pszFmt);
        vfprintf(LogFile, pszFmt, pVa);
        fflush(LogFile);
        va_end(pVa);
    }
}

/**
 * Read a line from stdin.
 */
int ReadLine(char *pszBuffer, int nCnt) {
    return fgets(pszBuffer, nCnt, stdin) != NULL;
}

/**
 * Convert an int representing a time in seconds to a string.
 */
char *FormatTime(unsigned long dwSecs, char *pszBuffer, size_t qwLen) {
    if (dwSecs >= 60 * ONE_SECOND) {
        long nMins;
        dwSecs = dwSecs / ONE_SECOND;
        nMins = dwSecs / 60;
        dwSecs -= nMins * 60;

        if (nMins >= 100)
            snprintf(pszBuffer, qwLen, "%ld:%02ld", nMins, dwSecs);
        else if (nMins >= 10)
            snprintf(pszBuffer, qwLen, " %ld:%02ld", nMins, dwSecs);
        else
            snprintf(pszBuffer, qwLen, "  %ld:%02ld", nMins, dwSecs);
    } else {
        int nTsecs = (dwSecs % ONE_SECOND) / 10;
        dwSecs = dwSecs / ONE_SECOND;

        snprintf(pszBuffer, qwLen, "  %2ld.%d", dwSecs, nTsecs);
    }
    return pszBuffer;
}

/**
 * Convert a score to a string.
 */
char *FormatScore(int nScore, char *pszBuffer, size_t qwLen) {
    if (nScore > CMLIMIT) {
        snprintf(pszBuffer, qwLen, "+M%d", (INF - nScore) / 2 + 1);
    } else if (nScore < -CMLIMIT) {
        snprintf(pszBuffer, qwLen, "-M%d", (nScore + INF) / 2);
    } else if (nScore == CMLIMIT) {
        snprintf(pszBuffer, qwLen, "+Mate");
    } else if (nScore == -CMLIMIT) {
        snprintf(pszBuffer, qwLen, "-Mate");
    } else if (nScore >= 0) {
        snprintf(pszBuffer, qwLen, "+%d.%03d", nScore / 1000, nScore % 1000);
    } else {
        snprintf(pszBuffer, qwLen, "-%d.%03d", (-nScore) / 1000, (-nScore) % 1000);
    }
    return pszBuffer;
}

/**
 * Convert a count to a string.
 *
 * Args:
 *     count: the count to format
 *     buffer: the buffer to write the formatted string to
 *     len: the length of the buffer
 *
 * Returns:
 *     the pointer to the buffer
 */
char *FormatCount(unsigned long dwCount, char *pszBuffer, size_t qwLen) {
    if (dwCount < 1000) {
        snprintf(pszBuffer, qwLen, "%lu", dwCount);
    } else if (dwCount < 10000ull) {
        double dScaled = dwCount * 1e-3;
        snprintf(pszBuffer, qwLen, "%.2fk", dScaled);
    } else if (dwCount < 100000ull) {
        double dScaled = dwCount * 1e-3;
        snprintf(pszBuffer, qwLen, "%.1fk", dScaled);
    } else if (dwCount < 1000000ull) {
        int nScaled = (int)(dwCount * 1e-3);
        snprintf(pszBuffer, qwLen, "%dk", nScaled);
    } else if (dwCount < 10000000ull) {
        double dScaled = dwCount * 1e-6;
        snprintf(pszBuffer, qwLen, "%.2fM", dScaled);
    } else if (dwCount < 100000000ull) {
        double dScaled = dwCount * 1e-6;
        snprintf(pszBuffer, qwLen, "%.1fM", dScaled);
    } else if (dwCount < 1000000000ull) {
        int nScaled = (int)(dwCount * 1e-6);
        snprintf(pszBuffer, qwLen, "%dM", nScaled);
    } else {
        double dScaled = dwCount * 1e-9;
        snprintf(pszBuffer, qwLen, "%.2fG", dScaled);
    }
    return pszBuffer;
}

/**
 * Get the current time.
 */
unsigned long GetTime(void) {
#if HAVE_GETTIMEOFDAY
    static struct timeval Timeval;

    gettimeofday(&Timeval, NULL);
    return Timeval.tv_sec * 100 + (Timeval.tv_usec / 10000L);
#else
#ifdef _WIN32
    return ((unsigned long)GetTickCount() / 10);
#else
#error TIME COUNTING MUST BE IMPLEMENTED
#endif
#endif
}

/**
 * Create a filename for a temporary file
 */
void GetTmpFileName(char *pszFileName, size_t qwLen) {
    for (int nCnt = 0;; nCnt++) {
        int nResult;
        struct stat Dummy;

        snprintf(pszFileName, qwLen, "save_%03d.pgn", nCnt);
        nResult = stat(pszFileName, &Dummy);

        if (nResult < 0)
            return;
    }
}
/**
 * Check if we can read from stdin without blocking.
 */
int InputReady(void) {
#if HAVE_SELECT
    fd_set Rfd;
    struct timeval Timeout;
    Timeout.tv_sec = Timeout.tv_usec = 0;
    FD_ZERO(&Rfd);
    FD_SET(0, &Rfd);

    return select(1, &Rfd, NULL, NULL, &Timeout) > 0;
#else
#ifdef _WIN32
    int nI;
    static int nInit = 0, pipe;
    static HANDLE hInh;
    DWORD dw;

    if (!XBoardMode && !_isatty(_fileno(stdin)))
        return (0);
    if (XBoardMode) {
#if defined(FILE_CNT)
        if (stdin->_cnt > 0)
            return stdin->_cnt;
#endif
        if (!nInit) {
            nInit = 1;
            hInh = GetStdHandle(STD_INPUT_HANDLE);
            pipe = !GetConsoleMode(hInh, &dw);
            if (!pipe) {
                SetConsoleMode(
                    hInh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
                FlushConsoleInputBuffer(hInh);
            }
        }
        if (pipe) {
            if (!PeekNamedPipe(hInh, NULL, 0, NULL, &dw, NULL)) {
                return 1;
            }
            return dw;
        } else {
            GetNumberOfConsoleInputEvents(hInh, &dw);
            return dw <= 1 ? 0 : dw;
        }
    } else {
        nI = _kbhit();
    }
    return (nI);
#else
    return 1;
#endif
#endif /* HAVE_SELECT */
}

/**
 * Tokenize a string.
 */
char *nextToken(char **pString, const char *pszDelim) {
    char *pszStart = *pString;
    char *pszEnd;
    const char *t;
    bool fFlag = true;

    if (pszStart == NULL)
        return NULL;

    while (fFlag) {
        fFlag = false;
        if (*pszStart == '\0')
            return NULL;
        for (t = pszDelim; *t; t++) {
            if (*t == *pszStart) {
                fFlag = true;
                pszStart++;
                break;
            }
        }
    }

    pszEnd = pszStart + 1;

    for (;;) {
        if (*pszEnd == '\0') {
            *pString = pszEnd;
            return pszStart;
        }
        for (t = pszDelim; *t; t++) {
            if (*t == *pszEnd) {
                *pszEnd = 0;
                *pString = pszEnd + 1;
                return pszStart;
            }
        }

        pszEnd++;
    }

    /* NEVER REACHED */
}

/**
 * Returns the ratio of dividend / divisor as percentage.
 * Handles some edge cases for convenience.
 */
int Percentage(unsigned long dwDividend, unsigned long dwDivisor) {
    if (dwDividend == 0) {
        return 0;
    }

    if (dwDivisor == 0) {
        return INT_MAX;
    }

    double dRatio = (double)dwDividend / (double)dwDivisor;
    return (int)(dRatio * 100.0 + 0.5);
}

char *strip(char *pszBuffer) {
    char *pszStart = pszBuffer;
    while (*pszStart == ' ') {
        pszStart++;
    }

    char *pszEnd = pszStart + strlen(pszStart) - 1;

    while (pszEnd > pszStart && *pszEnd == ' ') {
        *pszEnd = 0;
        pszEnd--;
    }

    return pszStart;
}
