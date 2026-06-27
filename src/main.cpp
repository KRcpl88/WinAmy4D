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
 * main.c - main program for Amy
 */

#include <ctype.h>
#include <string.h>

#include "evaluation_config.h"
#include "hashtable.h"
#include "init.h"
#include "learn.h"
#include "movedata.h"
#include "probe.h"
#include "random.h"
#include "recog.h"
#include "search.h"
#include "state_machine.h"
#include "test_blunder.h"
#include "test_dbase.h"
#include "test_yaml.h"
#include "utils.h"

static char CopyrightNotice[] =
    "    Amy version " VERSION ", Copyright (c) 2002-2026, Thorsten Greiner\n"
    "    Amy comes with ABSOLUTELY NO WARRANTY; for details type 'warranty'.\n"
    "    This is free software, and you are welcome to redistribute it\n"
    "    under certain conditions; type 'distribution' for details.\n\n"
    "    Amy contains table base access code which is copyrighted\n"
    "    by Eugene Nalimov and not free software.\n\n";

static char EGTBPath[1024] = "TB";

static char *ConfigFileName = NULL;

static void RunAllTests(void) {
    test_all_blunder();
    test_all_yaml();
    test_all_dbase();
}

static void ProcessOptions(int nArgc, char *pArgv[]) {
    int i;

    for (i = 1; i < nArgc; i++) {
        if (!strcmp(pArgv[i], "-ht")) {
            i++;
            if (i < nArgc) {
                GuessHTSizes(pArgv[i]);
            }
        }

        if (!strcmp(pArgv[i], "-conf")) {
            i++;
            if (i < nArgc) {
                ConfigFileName = pArgv[i];
            }
        }

        if (!strcmp(pArgv[i], "--test")) {
            RunAllTests();
            exit(0);
        }

        if (!strcmp(pArgv[i], "-debug")) {
            g_nDebugMode = 1;
            /*
             * An optional verbosity level may follow -debug. When the next
             * token is a number it is consumed and used to set g_nVerbosity;
             * otherwise the verbosity is left unchanged.
             */
            if (i + 1 < nArgc && isdigit((unsigned char)pArgv[i + 1][0])) {
                i++;
                g_nVerbosity = (uint16_t)atoi(pArgv[i]);
            }
        }
#if MP
        if (!strcmp(pArgv[i], "-cpu")) {
            i++;
            if (i < nArgc) {
                NumberOfCores = atoi(pArgv[i]);
            }
        }
#endif
    }
}

static void ProcessRCFile(void) {
    FILE *pRcFile = fopen(".amyrc", "r");
    char szBuf[1024];

    if (!pRcFile) {

        /*
         * Windows people have problems naming files .amyrc
         * So lets look for 'Amy.ini' too.
         */

        pRcFile = fopen("Amy.ini", "r");
    }

    if (!pRcFile)
        return;

    while (fgets(szBuf, 1023, pRcFile)) {
        char *pX = szBuf;
        char *pszKey, *pszValue;

        if (szBuf[0] == '#')
            continue;

        pszKey = nextToken(&pX, "=\t\n\r");
        if (pszKey == NULL)
            continue;

        pszValue = nextToken(&pX, "\n\n\r");
        if (pszValue == NULL)
            continue;

        if (!strcmp(pszKey, "ht")) {
            GuessHTSizes(pszValue);
        } else if (!strcmp(pszKey, "tbpath")) {
            strncpy(EGTBPath, pszValue, sizeof(EGTBPath) - 1);
        } else if (!strcmp(pszKey, "cpu")) {
#if MP
            NumberOfCores = atoi(pszValue);
#endif /* MP */
        } else if (!strcmp(pszKey, "autosave")) {
            AutoSave = !strcmp(pszValue, "true");
        }
    }

    fclose(pRcFile);
}

/**
 * Show the version of Amy.
 */
static void ShowVersion(void) {
    Print(0, "\n");
    Print(0, CopyrightNotice);
    Print(0, "\n\n");
}

int main(int nArgc, char *pArgv[]) {
#if HAVE_SETBUF
    setbuf(stdin, NULL);
#endif

    OpenLogFile("Amy.log");
    Print(0, "WinAmy: logging enabled\n");

    InitMoves();

    InitAll();
    HashInit();

    /*
     * Process rc file first, then command line options. This way command
     * line options can override rc file settings.
     */

    ProcessRCFile();
    ProcessOptions(nArgc, pArgv);

    ShowVersion();

    if (ConfigFileName) {
        LoadEvaluationConfig(ConfigFileName);
    }

    AllocateHT();
    InitEGTB(EGTBPath);
    RecogInit();

    DoBookLearning();

    Print(0, "\n");

    /* Ensure true random behavior. */
    InitRandom(GetTime());

    StateMachine();

    return 0;
}