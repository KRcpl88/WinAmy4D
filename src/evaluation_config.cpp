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

#include <stdint.h>
#include <stdio.h>

#include "dbase.h"
#include "evaluation.h"
#include "safe_malloc.h"
#include "scoord.h"
#include "search.h"
#include "utils.h"
#include "yaml.h"

extern int ExtendInCheck;
extern int ExtendDoubleCheck;
extern int ExtendDiscoveredCheck;
extern int ExtendSingularReply;

extern int ExtendPassedPawn;
extern int ExtendZugzwang;
extern int ReduceNullMove;
extern int ReduceNullMoveDeep;

extern int16_t ExtendRecapture[];



/** The name of the current configuration. */
const char *ConfigurationName = "default";

static void configure_name(struct YamlNode *);
static void configure_search(struct YamlNode *);
static void configure_pawn_scores(struct YamlNode *);
static void configure_knight_scores(struct YamlNode *);
static void configure_bishop_scores(struct YamlNode *);
static void configure_rook_scores(struct YamlNode *);
static void configure_queen_scores(struct YamlNode *);
static void configure_king_scores(struct YamlNode *);
static char *read_file(char *);
static void print_piece_square_table(FILE *, int16_t *);
static void print_array(FILE *, const char *, int16_t *, size_t);

/**
 * Reads evaluation parameters from a file.
 */
void LoadEvaluationConfig(char *pszFileName) {
    char *pszBuffer = read_file(pszFileName);

    if (pszBuffer == NULL)
        return;

    struct YamlNode *pNode = parse_yaml(pszBuffer);
    free(pszBuffer);

    if (pNode == NULL) {
        return;
    }

    configure_name(pNode);
    configure_search(pNode);
    configure_pawn_scores(pNode);
    configure_knight_scores(pNode);
    configure_bishop_scores(pNode);
    configure_rook_scores(pNode);
    configure_queen_scores(pNode);
    configure_king_scores(pNode);

    free_yaml_node(pNode);
}

/**
 * Writes evaluation config to a file.
 */
void SaveEvaluationConfig(char *pszFileName) {
    FILE *pFout = fopen(pszFileName, "w");

    if (!pFout) {
        perror("Cannot open file");
        return;
    }

    fprintf(pFout, "name: %s\n\n", ConfigurationName);

    fprintf(pFout, "search:\n");
    fprintf(pFout, "  extend_in_check: %d\n", ExtendInCheck);
    fprintf(pFout, "  extend_double_check: %d\n", ExtendDoubleCheck);
    fprintf(pFout, "  extend_discovered_check: %d\n", ExtendDiscoveredCheck);
    fprintf(pFout, "  extend_singular_reply: %d\n", ExtendSingularReply);
    fprintf(pFout, "  extend_passed_pawn: %d\n", ExtendPassedPawn);
    fprintf(pFout, "  extend_zugzwang: %d\n", ExtendZugzwang);
    fprintf(pFout, "  reduce_null_move: %d\n", ReduceNullMove);
    fprintf(pFout, "  reduce_null_move_deep: %d\n", ReduceNullMoveDeep);
    print_array(pFout, "extend_recapture", ExtendRecapture + 1, 5);
    fprintf(pFout, "\n");

    fprintf(pFout, "pawn:\n");
    fprintf(pFout, "  doubled: %d\n", DoubledPawn);
    fprintf(pFout, "  backward: %d\n", BackwardPawn);
    fprintf(pFout, "  hidden_backward: %d\n", HiddenBackwardPawn);
    fprintf(pFout, "  outruns_king: %d\n", PawnOutrunsKing);
    fprintf(pFout, "  blocked_development: %d\n", PawnDevelopmentBlocked);
    fprintf(pFout, "  duo: %d\n", PawnDuo);
    fprintf(pFout, "  storm: %d\n", PawnStorm);
    fprintf(pFout, "  cramping: %d\n", CrampingPawn);
    fprintf(pFout, "  majority: %d\n", PawnMajority);
    fprintf(pFout, "  covered_passed_pawn_6th_rank: %d\n", CoveredPassedPawn6th);
    fprintf(pFout, "  covered_passed_pawn_7th_rank: %d\n", CoveredPassedPawn7th);
    print_array(pFout, "passed", PassedPawn + 1, 6);
    print_array(pFout, "passed_blocked", PassedPawnBlocked + 1, 6);
    print_array(pFout, "passed_connected", PassedPawnConnected + 1, 6);
    print_array(pFout, "isolated", IsolatedPawn, 8);
    print_array(pFout, "advance_opening", PawnAdvanceOpening, 8);
    print_array(pFout, "advance_middle_game", PawnAdvanceMiddlegame, 8);
    print_array(pFout, "advance_end_game", PawnAdvanceEndgame, 8);
    print_array(pFout, "distant_passed", DistantPassedPawn, 35);
    print_array(pFout, "scale_half_open_files_mine", ScaleHalfOpenFilesMine, 5);
    print_array(pFout, "scale_half_open_files_yours", ScaleHalfOpenFilesYours,
                5);
    print_array(pFout, "scale_open_files", ScaleOpenFiles, 5);
    fprintf(pFout, "\n");

    fprintf(pFout, "knight:\n");
    fprintf(pFout, "  value: %d\n", Value[Knight]);
    fprintf(pFout, "  king_proximity: %d\n", KnightKingProximity);
    fprintf(pFout, "  blocks_c_pawn: %d\n", KnightBlocksCPawn);
    fprintf(pFout, "  edge_penalty: %d\n", KnightEdgePenalty);
    fprintf(pFout, "  piece_square_table: [\n");
    print_piece_square_table(pFout, KnightPos);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "  piece_square_table_outpost: [\n");
    print_piece_square_table(pFout, KnightOutpost);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "\n");

    fprintf(pFout, "bishop:\n");
    fprintf(pFout, "  value: %d\n", Value[Bishop]);
    fprintf(pFout, "  mobility: %d\n", BishopMobility);
    fprintf(pFout, "  king_proximity: %d\n", BishopKingProximity);
    fprintf(pFout, "  trapped: %d\n", BishopTrapped);
    print_array(pFout, "pair", BishopPair, 9);
    fprintf(pFout, "  piece_square_table: [\n");
    print_piece_square_table(pFout, BishopPos);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "\n");

    fprintf(pFout, "rook:\n");
    fprintf(pFout, "  value: %d\n", Value[Rook]);
    fprintf(pFout, "  mobility: %d\n", RookMobility);
    fprintf(pFout, "  on_open_file: %d\n", RookOnOpenFile);
    fprintf(pFout, "  on_semi_open_file: %d\n", RookOnSemiOpenFile);
    fprintf(pFout, "  king_proximity: %d\n", RookKingProximity);
    fprintf(pFout, "  connected: %d\n", RookConnected);
    fprintf(pFout, "  behind_passer: %d\n", RookBehindPasser);
    fprintf(pFout, "  on_7th_rank: %d\n", RookOn7thRank);
    fprintf(pFout, "  piece_square_table: [\n");
    print_piece_square_table(pFout, RookPos);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "\n");

    fprintf(pFout, "queen:\n");
    fprintf(pFout, "  value: %d\n", Value[Queen]);
    fprintf(pFout, "  king_proximity: %d\n", QueenKingProximity);
    fprintf(pFout, "  piece_square_table: [\n");
    print_piece_square_table(pFout, QueenPos);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "  piece_square_table_development: [\n");
    print_piece_square_table(pFout, QueenPosDevelopment);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "\n");

    fprintf(pFout, "king:\n");
    fprintf(pFout, "  blocks_rook: %d\n", KingBlocksRook);
    fprintf(pFout, "  in_center: %d\n", KingInCenter);
    fprintf(pFout, "  safety_scale: %d\n", KingSafetyScale);
    fprintf(pFout, "  piece_square_table_middle_game: [\n");
    print_piece_square_table(pFout, KingPosMiddlegame);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "  piece_square_table_end_game: [\n");
    print_piece_square_table(pFout, KingPosEndgame);
    fprintf(pFout, "    ]\n");
    fprintf(pFout, "  piece_square_table_end_game_queen_side: [\n");
    print_piece_square_table(pFout, KingPosEndgameQueenSide);
    fprintf(pFout, "    ]\n");

    fclose(pFout);

    Print(0, "Saved configuration '%s' to '%s'.\n", ConfigurationName,
          pszFileName);
}

/**
 * Writes a piece-square table to file fout.
 */
static void print_piece_square_table(FILE *pFout, int16_t *pPieceSquareTable) {
    for (unsigned int dwOffset = 0; dwOffset < CBitBoard::SIZE; dwOffset++) {
        const CSCoord square(static_cast<int>(dwOffset));
        if (square.m_nFile == 0) {
            fprintf(pFout, "    ");
        }
        fprintf(pFout, "%5d, ", (int)pPieceSquareTable[static_cast<int>(square)]);
        if (square.m_nFile == 7) {
            fprintf(pFout, "\n");
        }
    }
}

/**
 * Writes an array to file fout in a single line.
 */
static void print_array(FILE *pFout, const char *pszPrefix, int16_t *pArray,
                        size_t qwCount) {
    fprintf(pFout, "  %s: [", pszPrefix);
    for (size_t i = 0; i < qwCount; i++) {
        fprintf(pFout, "%d, ", (int)pArray[i]);
        if ((i % 10) == 9) {
            fprintf(pFout, "\n    ");
        }
    }
    fprintf(pFout, "]\n");
}

/**
 * Set a named parameter.
 */
static void set_parameter(struct YamlNode *pNode, const char *pszName, int *pParameter) {
    struct IntLookupResult result = get_as_int(pNode, pszName);
    if (result.result_code == OK) {
        Print(9, "%s: %d\n", pszName, result.result);
        *pParameter = result.result;
    }
}

static void set_piece_square_table(struct YamlNode *pNode, const char *pszName,
                                   int16_t *pTargetTable) {
    int rgPieceSquareTable[CBitBoard::SIZE];

    struct IntArrayLookupResult ArrayResult =
        get_as_int_array(pNode, pszName, rgPieceSquareTable, CBitBoard::SIZE);

    if (ArrayResult.result_code == OK) {
        if (ArrayResult.elements_read != static_cast<int>(CBitBoard::SIZE)) {
            Print(0, "Warning: expected %d entries for %s, got %d!\n",
                  static_cast<int>(CBitBoard::SIZE), pszName, ArrayResult.elements_read);
        }
        Print(9, "%s:\n", pszName);
        for (unsigned int i = 0; i < ArrayResult.elements_read; i++) {
            pTargetTable[i] = (int16_t)rgPieceSquareTable[i];
            Print(9, "%5d, ", rgPieceSquareTable[i]);
            if (i % CBitBoard::MAX_LEVEL_WIDTH == (CBitBoard::MAX_LEVEL_WIDTH - 1)) {
                Print(9, "\n");
            }
        }
    }
}

static void set_array(struct YamlNode *pNode, const char *pszName, int16_t *pTargetArray,
                      unsigned int dwCount) {
    int *pDestination = (int *)safe_malloc(sizeof(int) * dwCount);

    struct IntArrayLookupResult ArrayResult =
        get_as_int_array(pNode, pszName, pDestination, dwCount);

    if (ArrayResult.result_code == OK) {
        if (ArrayResult.elements_read != dwCount) {
            Print(0, "Warning: expected %d entries for %s, got %d!\n", dwCount,
                  pszName, ArrayResult.elements_read);
        }
        Print(9, "%s: ", pszName);
        for (unsigned int i = 0; i < ArrayResult.elements_read; i++) {
            pTargetArray[i] = (int16_t)pDestination[i];
            Print(9, "%d, ", pDestination[i]);
        }
        Print(9, "\n");
    }

    free(pDestination);
}

static void configure_name(struct YamlNode *pNode) {
    struct StringLookupResult result = get_as_string(pNode, "name");

    if (result.result_code == OK) {
        ConfigurationName = result.result;
        Print(0, "Using configuration name: %s\n", ConfigurationName);
    }
}

static void configure_search(struct YamlNode *pNode) {
    set_parameter(pNode, "search.extend_in_check", &ExtendInCheck);
    set_parameter(pNode, "search.extend_double_check", &ExtendDoubleCheck);
    set_parameter(pNode, "search.extend_discovered_check",
                  &ExtendDiscoveredCheck);
    set_parameter(pNode, "search.extend_singular_reply", &ExtendSingularReply);
    set_parameter(pNode, "search.extend_passed_pawn", &ExtendPassedPawn);
    set_parameter(pNode, "search.extend_zugzwang", &ExtendZugzwang);
    set_parameter(pNode, "search.reduce_null_move", &ReduceNullMove);
    set_parameter(pNode, "search.reduce_null_move_deep", &ReduceNullMoveDeep);
    set_array(pNode, "search.extend_recapture", ExtendRecapture + 1, 5);
}

static void configure_pawn_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "pawn.doubled", &DoubledPawn);
    set_parameter(pNode, "pawn.backward", &BackwardPawn);
    set_parameter(pNode, "pawn.hidden_backward", &HiddenBackwardPawn);
    set_parameter(pNode, "pawn.outruns_king", &PawnOutrunsKing);
    set_parameter(pNode, "pawn.blocked_development", &PawnDevelopmentBlocked);
    set_parameter(pNode, "pawn.duo", &PawnDuo);
    set_parameter(pNode, "pawn.storm", &PawnStorm);
    set_parameter(pNode, "pawn.cramping", &CrampingPawn);
    set_parameter(pNode, "pawn.majority", &PawnMajority);
    set_parameter(pNode, "pawn.covered_passed_pawn_6th_rank",
                  &CoveredPassedPawn6th);
    set_parameter(pNode, "pawn.covered_passed_pawn_7th_rank",
                  &CoveredPassedPawn7th);
    set_array(pNode, "pawn.passed", PassedPawn + 1, 6);
    set_array(pNode, "pawn.passed_blocked", PassedPawnBlocked + 1, 6);
    set_array(pNode, "pawn.passed_connected", PassedPawnConnected + 1, 6);
    set_array(pNode, "pawn.isolated", IsolatedPawn, 8);
    set_array(pNode, "pawn.advance_opening", PawnAdvanceOpening, 8);
    set_array(pNode, "pawn.advance_middle_game", PawnAdvanceMiddlegame, 8);
    set_array(pNode, "pawn.advance_end_game", PawnAdvanceEndgame, 8);
    set_array(pNode, "pawn.distant_passed", DistantPassedPawn, 35);
    set_array(pNode, "pawn.scale_half_open_files_mine", ScaleHalfOpenFilesMine,
              5);
    set_array(pNode, "pawn.scale_half_open_files_yours", ScaleHalfOpenFilesYours,
              5);
    set_array(pNode, "pawn.scale_open_files", ScaleOpenFiles, 5);
}

static void configure_knight_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "knight.value", &Value[Knight]);
    set_parameter(pNode, "knight.king_proximity", &KnightKingProximity);
    set_parameter(pNode, "knight.blocks_c_pawn", &KnightBlocksCPawn);
    set_parameter(pNode, "knight.edge_penalty", &KnightEdgePenalty);

    set_piece_square_table(pNode, "knight.piece_square_table", KnightPos);
    set_piece_square_table(pNode, "knight.piece_square_table_outpost",
                           KnightOutpost);
}

static void configure_bishop_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "bishop.value", &Value[Bishop]);
    set_parameter(pNode, "bishop.mobility", &BishopMobility);
    set_parameter(pNode, "bishop.king_proximity", &BishopKingProximity);
    set_parameter(pNode, "bishop.trapped", &BishopTrapped);

    set_array(pNode, "bishop.pair", BishopPair, 9);
    set_piece_square_table(pNode, "bishop.piece_square_table", BishopPos);
}

static void configure_rook_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "rook.value", &Value[Rook]);
    set_parameter(pNode, "rook.mobility", &RookMobility);
    set_parameter(pNode, "rook.on_open_file", &RookOnOpenFile);
    set_parameter(pNode, "rook.on_semi_open_file", &RookOnSemiOpenFile);
    set_parameter(pNode, "rook.king_proximity", &RookKingProximity);
    set_parameter(pNode, "rook.connected", &RookConnected);
    set_parameter(pNode, "rook.behind_passer", &RookBehindPasser);
    set_parameter(pNode, "rook.on_7th_rank", &RookOn7thRank);
    set_piece_square_table(pNode, "rook.piece_square_table", RookPos);
}

static void configure_queen_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "queen.value", &Value[Queen]);
    set_parameter(pNode, "queen.king_proximity", &QueenKingProximity);
    set_piece_square_table(pNode, "queen.piece_square_table", QueenPos);
    set_piece_square_table(pNode, "queen.piece_square_table_development",
                           QueenPosDevelopment);
}

static void configure_king_scores(struct YamlNode *pNode) {
    set_parameter(pNode, "king.blocks_rook", &KingBlocksRook);
    set_parameter(pNode, "king.in_center", &KingInCenter);
    set_parameter(pNode, "king.safety_scale", &KingSafetyScale);
    set_piece_square_table(pNode, "king.piece_square_table_middle_game",
                           KingPosMiddlegame);
    set_piece_square_table(pNode, "king.piece_square_table_end_game",
                           KingPosEndgame);
    set_piece_square_table(pNode, "king.piece_square_table_end_game_queen_side",
                           KingPosEndgameQueenSide);
}

static char *read_file(char *pszFileName) {
    FILE *pFin = fopen(pszFileName, "r");
    if (!pFin) {
        perror("Cannot open file");
        return NULL;
    }

    const size_t qwPageSize = 1024;
    size_t qwBufSize = qwPageSize;
    size_t qwTotalBytesRead = 0;

    char *pszBuffer = (char *)safe_malloc(qwBufSize);

    char *pszPtr = pszBuffer;

    for (;;) {
        size_t qwBytesRead = fread(pszPtr, 1, qwPageSize, pFin);

        if (qwBytesRead == 0)
            break;

        pszPtr += qwBytesRead;
        qwTotalBytesRead += qwBytesRead;

        if ((qwTotalBytesRead + qwPageSize) >= qwBufSize) {
            qwBufSize *= 2;
            pszBuffer = (char *)safe_realloc(pszBuffer, qwBufSize);
            pszPtr = pszBuffer + qwTotalBytesRead;
        }
    }

    fclose(pFin);

    if ((qwTotalBytesRead + 1) >= qwBufSize) {
        qwBufSize += 1;
        pszBuffer = (char *)safe_realloc(pszBuffer, qwBufSize);
    }
    *pszPtr = '\0';

    return pszBuffer;
}
