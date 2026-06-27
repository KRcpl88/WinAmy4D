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
 * time_ctl.c - time management routines
 */

#include "amy.h"

#include <stdio.h>
#include <string.h>

#include "dbase.h"
#include "time_ctl.h"
#include "utils.h"

int TMoves = 250, TTime = 2 * 60 * 60;
int Moves[3] = {250, 250}, Time[3] = {2 * 60 * 60, 2 * 60 * 60};
int TMoves2, TTime2;
int TwoTimeControls = false;

int Increment = 0;

struct SingleTimeControl {
    int moves;
    int total_time;
    int increment;
};

struct TimeControl {
    struct SingleTimeControl first;
    struct SingleTimeControl second;
    bool hasSecondTimeControl;
};

/** Stores the single global time control. */
static struct TimeControl globalTimeControl = {
    {250, 2 * 60 * 60, 0},
    {0, 0, 0},
    false,
};

void DoTC(CPosition *p, int nMtime) {
    Time[p->GetTurn()] += -nMtime + Increment;

    if (Moves[p->GetTurn()] > 0) {
        Moves[p->GetTurn()] -= 1;
        if (Moves[p->GetTurn()] <= 0) {
            if (TwoTimeControls) {
                Print(0, "Switching to second time control.\n");
                TMoves = TMoves2;
                TTime = TTime2;
                TwoTimeControls = false;
            }
            Moves[p->GetTurn()] = TMoves;
            Time[p->GetTurn()] += TTime;
        }
    }
}

void CalcTime(CPosition *p, float *pSoft, float *pHard) {
    char szTimeAsText[16];
    if (TMoves >= 0) {
        if (Moves[p->GetTurn()] > 0) {
            /*  int limit = (13*TTime/TMoves)/8 + (3*Increment)/4;  */
            float fLimit = (1.625f * TTime / TMoves) + (0.85f * Increment);

            Print(1, "TC: %d moves in %s\n", Moves[p->GetTurn()],
                  FormatTime((unsigned int)(Time[p->GetTurn()]) * ONE_SECOND,
                             szTimeAsText, sizeof(szTimeAsText)));
            /*  *soft = (7*Time[p->GetTurn()]/Moves[p->GetTurn()])/8 + (3*Increment)/4; */
            *pSoft =
                (0.875f * Time[p->GetTurn()] / Moves[p->GetTurn()]) + (0.75f * Increment);
            if (*pSoft > fLimit)
                *pSoft = fLimit;

            if (TwoTimeControls && Moves[p->GetTurn()] <= 5) {
                int nMoves = TMoves2;
                int nSoft2;
                if (nMoves == 0)
                   nMoves = 60;
                nSoft2 = TTime2 / nMoves;
                /*    *soft = ((*soft)+(float)soft2)/2;    */
                *pSoft = 0.5f * ((*pSoft) + (float)nSoft2);
                Print(1, "Adjusted timing to %.4f secs\n", *pSoft);
            }
            *pHard = 4.0f * (*pSoft);
        } else {
            /*  expect additional game length of 60 moves beyond current move */
            /*  use equations from section above with fixed Moves[p->GetTurn()] of 60
             */
            /*  rearrange equation to eliminate floating point division  */
            /*  1.625 / 60 = 0.0271  */
            float fLimit = 0.0271f * ((float)Time[p->GetTurn()] + 60 * Increment);

            Print(1, "TC: all moves in %s\n",
                  FormatTime((unsigned int)(Time[p->GetTurn()]) * ONE_SECOND,
                             szTimeAsText, sizeof(szTimeAsText)));
            /*  0.875 / 60.0 = 0.0146  */
            *pSoft = 0.0146f * (float)Time[p->GetTurn()] + (0.85f * Increment);
            if (*pSoft > fLimit)
                *pSoft = fLimit;
            *pHard = 4.0f * (*pSoft);
        }
        if (*pHard > Time[p->GetTurn()])
            *pHard = 0.5f * (float)Time[p->GetTurn()];
        if (*pSoft > *pHard)
            *pSoft = 0.67f * (*pHard);

        Print(1, "TL: %.2f/%.2f\n", *pSoft, *pHard);
    } else {
        *pSoft = *pHard = (float)TTime;
    }
}

static struct TimeControl parse_timecontrol_xboard(char *rgArgs[]) {
    int nTtmoves, nTtime, nTminutes, nTseconds, nInc = 0;

    sscanf(rgArgs[0], "%d", &nTtmoves);
    char *pszColon = strchr(rgArgs[1], ':'); /* check for time in xx:yy format */
    if (pszColon) {
        sscanf(rgArgs[1], "%d:%d", &nTminutes, &nTseconds);
        nTtime = (nTminutes * 60) + nTseconds;
    } else {
        sscanf(rgArgs[1], "%d", &nTminutes);
        nTtime = nTminutes * 60;
    }
    sscanf(rgArgs[2], "%d", &nInc);

    struct TimeControl result = {};
    result.first.moves = nTtmoves;
    result.first.total_time = nTtime;
    result.first.increment = nInc;
    result.hasSecondTimeControl = false;

    return result;
}

/** Literal for 'sudden death' time control. */
static const char *const sudden_death = "sd";

/** Literal for 'fixed' time control. */
static const char *const fixed = "fixed";

static struct TimeControl parse_timecontrol(char *rgArgs[]) {
    int nTtmoves, nTtime, nInc = 0;
    int nTtmoves2, nTtime2;

    struct TimeControl result = globalTimeControl;

    char *x = strtok(rgArgs[0], "/+ \t\n\r");
    if (x) {
        if (!strcmp(x, sudden_death))
            nTtmoves = 0;
        else if (!strcmp(x, fixed))
            nTtmoves = -1;
        else
            sscanf(x, "%d", &nTtmoves);
        x = strtok(NULL, "/ \t\n\r");
        if (x) {
            sscanf(x, "%d", &nTtime);
            for (x++; *x; x++) {
                if (*x == '+') {
                    sscanf(x + 1, "%d", &nInc);
                    break;
                }
            }
            if (rgArgs[1] != NULL) {
                x = strtok(rgArgs[1], " /\n\t\r");
                nTtmoves2 = -1;
                if (!strcmp(x, sudden_death))
                    nTtmoves2 = 0;
                else
                    sscanf(x, "%d", &nTtmoves2);
                x = strtok(NULL, " /\n\t\r");
                if (x) {
                    nTtime2 = -1;
                    sscanf(x, "%d", &nTtime2);
                    if (nTtmoves2 >= 0 && nTtime2 > 0)
                        TwoTimeControls = true;
                }
            }
            if (nTtmoves >= 0) {
                result.first.moves = nTtmoves;
                result.first.total_time = nTtime * 60;
                result.first.increment = nInc;
                result.hasSecondTimeControl = false;

            } else {
                result.first.moves = -1;
                result.first.total_time = nTtime;
                result.first.increment = 0;
                result.hasSecondTimeControl = false;
            }
            if (TwoTimeControls) {
                result.second.moves = nTtmoves2;
                result.second.total_time = nTtime2 * 60;
                result.second.increment = 0;
                result.hasSecondTimeControl = true;
            }
        }
    }
    return result;
}

/**
 * Set the global time control using an array of strings.
 *
 * Args:
 *     args: an array of strings for the time controls
 *     xboard_flag: indicates whether the format is expected to be
 *         for xboard or not
 */
void SetTimeControl(char *rgArgs[], bool fXboardFlag) {
    if (fXboardFlag) {
        globalTimeControl = parse_timecontrol_xboard(rgArgs);
    } else {
        globalTimeControl = parse_timecontrol(rgArgs);
    }
    ResetTimeControl(!fXboardFlag);
}

/**
 * Reset the global time control and clocks.
 *
 * Args:
 *     verbose: if true, the settings are printed
 */
void ResetTimeControl(bool fVerbose) {
    TMoves = globalTimeControl.first.moves;
    TTime = globalTimeControl.first.total_time;
    Increment = globalTimeControl.first.increment;

    TwoTimeControls = globalTimeControl.hasSecondTimeControl;

    TMoves2 = globalTimeControl.second.moves;
    TTime2 = globalTimeControl.second.total_time;

    Moves[White] = Moves[Black] = TMoves;
    Time[White] = Time[Black] = TTime;

    if (fVerbose) {
        Print(0, "Timecontrol is ");
        if (globalTimeControl.first.moves >= 0) {
            if (globalTimeControl.first.moves == 0)
                Print(0, "all ");
            else
                Print(0, "%d ", globalTimeControl.first.moves);

            if (globalTimeControl.first.increment) {
                Print(0, "moves in %d mins + %d secs Increment\n",
                      globalTimeControl.first.total_time / 60,
                      globalTimeControl.first.increment);
            } else {
                Print(0, "moves in %d mins\n",
                      globalTimeControl.first.total_time / 60);
            }

        } else {
            Print(0, "%d seconds/move fixed time\n",
                  globalTimeControl.first.total_time);
        }

        if (globalTimeControl.hasSecondTimeControl) {
            Print(0, "Second Timecontrol is ");
            if (globalTimeControl.second.moves == 0)
                Print(0, "all ");
            else
                Print(0, "%d ", globalTimeControl.second.moves);
            Print(0, "moves in %d mins\n",
                  globalTimeControl.second.total_time / 60);
        }
    }
}

/**
 * Configure a fixed time-per-move time control.
 *
 * The engine will spend (at most) the given number of seconds on each search.
 * Internally this uses the existing "fixed" time-control mode (moves == -1),
 * for which CalcTime() returns the seconds-per-move value directly as both the
 * soft and hard search limit.
 *
 * Args:
 *     seconds: the per-move time budget in seconds (must be > 0).
 */
void SetFixedTimePerMove(int nSeconds) {
    if (nSeconds <= 0) {
        return;
    }
    globalTimeControl.first.moves = -1;
    globalTimeControl.first.total_time = nSeconds;
    globalTimeControl.first.increment = 0;
    globalTimeControl.second.moves = 0;
    globalTimeControl.second.total_time = 0;
    globalTimeControl.second.increment = 0;
    globalTimeControl.hasSecondTimeControl = false;
    ResetTimeControl(false);
}

/**
 * Restore the default time control (250 moves in 2 hours).
 *
 * Used to leave fixed time-per-move mode so that search termination is governed
 * by the configured search depth rather than a wall-clock budget.
 */
void SetDefaultTimeControl(void) {
    globalTimeControl.first.moves = 250;
    globalTimeControl.first.total_time = 2 * 60 * 60;
    globalTimeControl.first.increment = 0;
    globalTimeControl.second.moves = 0;
    globalTimeControl.second.total_time = 0;
    globalTimeControl.second.increment = 0;
    globalTimeControl.hasSecondTimeControl = false;
    ResetTimeControl(false);
}