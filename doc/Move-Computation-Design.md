# WinAmy Move Computation Design (Enumeration → Evaluation → Selection)

This document describes how WinAmy computes the optimal move for the side to move, mapped to the 3 requested steps:

1. Enumerating all possible moves from the current position  
2. Evaluating and scoring each move  
3. Choosing the best next move

---

## Main entry points and high-level flow

Primary game-search entry points:

- `CPosition::SearchRoot()` (`src/search.cpp:1798`)  
- `CPosition::Iterate()` (`src/search.cpp:1717`)  
- `IterateInt()` (internal iterative-deepening loop, `src/search.cpp:1248`)

High-level control:

1. `SearchRoot()` optionally picks a book move (`SelectBook`), else clones the position and calls `Iterate()` (`src/search.cpp:1805-1823`).
2. `Iterate()` does pre-search setup (time controls, legal move count, evaluation/hash init), creates `CSearchData`, then runs `IterateInt(sd)` (`src/search.cpp:1730-1776`).
3. `IterateInt()` searches root moves depth-by-depth with `NegaScout()`/`Quies()` and stores final best move in `sd->m_BestMove` (`src/search.cpp:1278-1643`).

---

## 1 - Move Enumeration

Strict legal generation at root (`LegalMoves` + `GenTo/GenFrom/GenEnpas` + legality filter), and phased incremental generation in search (`NextMove`/`NextEvasion`/`NextMoveQ`).

### 1.1 Core classes/structs used

- `CPosition` (`include/dbase.h:120`): board state, turn, attack maps, piece masks, move application/rollback methods.
- `CMove` (`include/move.h:7`): move representation (from/to + flags: capture, castle, en passant, promotion, etc.).
- `heap_t` move buffers: used to store generated moves.
- `CSearchData` (`include/searchdata.h:69`) + `SSearchStatus` (`include/searchdata.h:54`): per-search generator phase state (`SearchPhase` enum).

### 1.2 Legal move enumeration at root

At search start, `Iterate()` first checks legal move count:

- `p->LegalMoves(heap)` (`src/search.cpp:1733`)
- If 0: mate/stalemate early return (`src/search.cpp:1744-1751`)
- If 1: forced move early return (`src/search.cpp:1751-1756`)

`CPosition::LegalMoves()` (`src/dbase.cpp:2143`) calls `legal_moves_internal()` (`src/dbase.cpp:2076`) which:

1. Generates candidate captures using `GenTo()` for each enemy-occupied target square (`src/dbase.cpp:2079-2099`).
2. Generates candidate non-captures using `GenFrom()` for each own piece square (`src/dbase.cpp:2101-2124`).
3. Generates en-passant candidates via `GenEnpas()` (`src/dbase.cpp:2126-2140`).
4. Filters for strict legality by:
   - `DoMove(move)`
   - `!InCheck(OPP(m_nTurn))` test
   - `UndoMove(move)`  
   (`src/dbase.cpp:2091-2096`, `2116-2120`, `2133-2137`)

So enumeration is not just pseudo-legal generation; final root move list is strictly legal.

### 1.3 Incremental move enumeration inside tree search

Inside `NegaScout()`, moves are not generated all at once; they are produced incrementally:

- `move = incheck ? sd->NextEvasion() : sd->NextMove()` (`src/search.cpp:824`)

`CSearchData::NextMove()` (`src/search_data.cpp:159`) is a **search-ordering iterator** over legal candidates for normal nodes. It does not define piece geometry (for example, “bishop moves diagonally”); that geometry is precomputed elsewhere and consumed by lower-level generators. `NextMove()` controls *when* each move class is produced so alpha-beta can cut quickly:

1. `HashMove`
2. `GenerateCaptures`
3. `GainingCapture` (SEE/`SwapOff` >= 0)
4. `Killer1`, `Killer2`
5. `CounterMv`
6. `Killer3`
7. `LoosingCapture`
8. `GenerateRest` (quiet moves + castling + pawn pushes)
9. `HistoryMoves`  
(`SearchPhase` in `include/searchdata.h:38-52`, logic in `src/search_data.cpp:166-448`)

`NextEvasion()` (`src/search_data.cpp:456`) is the in-check companion iterator. It uses similar staging but limits candidates to legal evasions (king moves, captures of the checking piece, and interpositions when applicable).

`NextMoveQ()` (`src/search_data.cpp:905`) is the quiescence iterator. It is tactical-only (primarily captures/check continuations) and is built from `GenerateQCaptures()` (`src/search_data.cpp:761`) so `Quies()` searches forcing lines rather than all quiet continuations.

### 1.4 Low-level generators

- `GenTo(square, heap)` capture generation to a target (`src/dbase.cpp:950-967`)
- `GenFrom(square, heap)` non-capture generation from a piece square (`src/dbase.cpp:988-1047`)
- `GenEnpas(heap)` en-passant generation (`src/dbase.cpp:969-982`)
- `MayCastle(move)` validates castling path/attack conditions (`src/dbase.cpp:1053-1099`)

These are built on precomputed attack maps in `CPosition` (`m_rgAtkTo`, `m_rgAtkFr`) and piece masks (`m_rgMask`).

### 1.5 Piece-move geometry and special-move rules in the pipeline

`GenTo` and `GenFrom` are `CPosition` methods (declared in `include/dbase.h`, implemented in `src/dbase.cpp`) that convert attack/mask information into move objects.  
After the refactor, attack geometry is computed at runtime using `ATTACK_DELTA` and `CSCoord::Step`, and `magic.cpp`/magic-table lookup is removed.

Current geometry sources and attack flow:

- Piece direction tables are defined as `ATTACK_DELTA` with per-piece counts in `ATTACK_DELTA_COUNT` (`include/dbase.h`, `src/dbase.cpp`).
- `ComputeSlidingAttacks(const CSCoord&, int, const CBitBoard&)` ray-walks each piece direction with repeated `Step(dir)` until off-board (`!IsValid()`) or blocked (`occupied.TstBit(...)`) (`src/dbase.cpp`).
- `ComputeLeapAttacks(const CSCoord&, int)` applies single-step deltas for non-sliding pieces (`src/dbase.cpp`).
- `AtkSet(...)` selects the model per piece:
  - pawn/knight/king → `ComputeLeapAttacks`
  - bishop/rook/queen → `ComputeSlidingAttacks`
  and writes the result into `m_rgAtkTo` / `m_rgAtkFr` (`src/dbase.cpp`).
- `GenTo`/`GenFrom`/`LegalMoves` then enumerate candidates from these attack maps and enforce strict legality via `DoMove` + `!InCheck` + `UndoMove`.

#### How attack computation now works for a specific `CPosition`

For a piece on square `sq` in position `p`:

1. `AtkSet` computes `occupied = p->m_rgMask[0][0] | p->m_rgMask[1][0]`.
2. It dispatches to `ComputeLeapAttacks` (pawn/knight/king) or `ComputeSlidingAttacks` (bishop/rook/queen).
3. `ComputeSlidingAttacks` iterates each direction from `ATTACK_DELTA[pieceType]`, stepping square-by-square with `CSCoord::Step(dir)`:
   - stop when `Step` yields an invalid square (board edge / invalid transition),
   - include each valid square in attacks,
   - stop ray on first occupied square.
4. `ComputeLeapAttacks` steps once per direction and includes only valid destinations.
5. `AtkSet` stores attacks in `m_rgAtkTo[sq]` and updates reverse map `m_rgAtkFr[*]`.
6. Move generators apply side/occupancy constraints and legal filtering as before.

#### Queen legal-move generation after the refactor

Queen attacks are now generated directly by `ComputeSlidingAttacks(squareCoord, Queen, occupied)` using the queen rows in `ATTACK_DELTA`, rather than by unioning magic bishop/rook tables.

That attack set is consumed by the normal generation and legality pipeline (`GenTo`/`GenFrom`, then `DoMove`/`InCheck`/`UndoMove`), so final queen moves remain strictly legal.

Special rules are integrated into the same generation/legality/application pipeline rather than a separate subsystem:

- Promotion candidates are emitted in `GenTo`/`GenFrom` using `make_promotion(...)` and `is_promo_square(...)` (`src/dbase.cpp:958-963`, `1027-1031`, `include/inline.h:120-138`), then materialized/reverted in `DoMove`/`UndoMove` via `PromoType(...)` (`src/dbase.cpp:577-589`, `698-713`).
- Castling candidates are produced in `GenFrom` and search `GenerateRest` (`src/dbase.cpp:1006-1017`, `src/search_data.cpp:354-363`), validated by `MayCastle` (`src/dbase.cpp:1053-1099`), and applied/reverted through `DoCastle`/`UndoCastle` from `DoMove`/`UndoMove` (`src/dbase.cpp:332-448`, `469-472`, `682-684`).
- Pawn two-square advances (`M_PAWND`) are computed with blocker checks in both root and staged generators:
  - Root legal list path: `GenFrom` first requires the one-step square to be empty, then requires the two-step square to be empty before appending `M_PAWND` (`src/dbase.cpp:1025-1041`).
  - Search staged path: `GenerateRest` and `NextEvasion` do the same in bitboard form by shifting one rank, masking with `empty`, restricting to start-rank pawns (`ThirdRank`), shifting again, then masking with `empty` before appending `M_PAWND` (`src/search_data.cpp:378-410`, `683-721`).
  - Final legality guard: `LegalMove` rechecks midpoint and destination emptiness for `move.IsPawnDoublePush()` (`src/dbase.cpp:1177-1186`).
- En-passant is generated by `GenEnpas` (`src/dbase.cpp:969-982`), checked in `LegalMove` (`src/dbase.cpp:1177-1234`), and applied/reverted in `DoMove`/`UndoMove` (`src/dbase.cpp:537-572`, `735-762`), with EP-target state set on pawn double pushes (`src/dbase.cpp:620-628`).

---

## 2 — Evaluating and scoring each move

WinAmy scores moves using recursive search scores (primary), with static evaluation used at quiescence leaves and additional heuristic scoring for ordering/pruning.

 - Recursive alpha-beta/Negascout scoring (`NegaScout`), quiescence tactical scoring (`Quies`), static position evaluation (`EvaluatePosition`), plus move-ordering heuristics (SEE, killer/history/counter).

### 2.1 Core classes/structs used

- `CSearchData`: search state and scoring context:
  - killer/history/counter tables (`m_pKillerTable`, `m_rguHistoryTab`, `m_rgCounterTab`)
  - principal variation scratch (`m_rgPvSave`)
  - search metrics and result fields (`m_nBestScore`, `m_BestMove`)  
  (`include/searchdata.h:73-104`)
- `CPosition`: mutable position searched via `DoMove`/`UndoMove`.
- `PawnFacts` (`include/evaluation.h:38`) and evaluation hash tables used by `EvaluatePosition`.

The killer-move heuristic is one of these ordering tools: a **non-tactical move** that previously caused a **beta cutoff** at the same ply is stored in `SKillerEntry` (`killer1`/`killer2` + hit counters in `include/searchdata.h:60-63`) via `CSearchData::PutKiller` (`src/search_data.cpp:959-988`). On later sibling nodes at that ply, the move is tried early in `Killer1`/`Killer2`/`Killer3` phases (`src/search_data.cpp:248-299`, `539-590`) to increase pruning efficiency.

### 2.2 Recursive scoring in full search (`NegaScout`)

`CSearchData::NegaScout()` (`src/search.cpp:578`) computes move scores by:

1. Node prechecks: termination (`TerminateSearch`), depth, repetition, in-check extension (`src/search.cpp:610-638`).
2. TT probe (`ProbeHT`) for exact/bound hits (`src/search.cpp:643-681`).
3. Optional EGTB/recognizer probes (`src/search.cpp:687-712`).
4. Optional null-move pruning (`src/search.cpp:722-775`).
5. Iterate legal candidates from `NextMove`/`NextEvasion` (`src/search.cpp:824`).
6. For each candidate:
   - apply dynamic extensions/reductions (recapture, passed pawn, check, futility)
   - `DoMove`
   - reject illegal resulting positions (`InCheck(OPP(m_nTurn))`)
   - recurse (`NegaScout` or `Quies`)
   - `UndoMove`  
   (`src/search.cpp:824-965`)
7. Handle beta cutoffs and update killer/counter/history via:
   - `PutKiller`
   - `StoreResult`  
   (`src/search.cpp:986-992`, `src/search.cpp:461-476`)

`ScoreMove()` (`src/search.cpp:435`) provides optimistic tactical values used by futility pruning (`src/search.cpp:873-912`), not final evaluation.

### 2.3 Quiescence scoring (`Quies`)

`CSearchData::Quies()` (`src/search.cpp:486`) evaluates tactical continuations when full depth is exhausted:

1. Enter node, depth/repetition checks (`src/search.cpp:494-503`).
2. Recognizer probe, else static evaluation `EvaluatePosition(p)` (`src/search.cpp:512-530`).
3. If needed, search tactical replies from `NextMoveQ(alpha)` recursively (`src/search.cpp:539-556`).

### 2.4 Static evaluation (`EvaluatePosition`)

`EvaluatePosition()` (`src/evaluation.cpp:1775`) returns side-to-move-correct signed score, delegating to `EvaluatePositionForWhite()` (`src/evaluation.cpp:1264`).

Main components in `EvaluatePositionForWhite()` include:

- Material (`MaterialBalance`, `src/evaluation.cpp:1254`)
- Pawn structure (`EvaluatePawnsHashed`)
- King safety (`EvaluateKingSafety`)
- Passed pawns (`EvaluatePassedPawns`)
- Development and piece-specific terms (knight/bishop/rook/queen/king PST and mobility)  
(`src/evaluation.cpp:1290-1767`)

Pre-search eval setup:

- `InitEvaluation(p)` called in `Iterate()` and `QuiescenceSearch()` (`src/search.cpp:1764`, `1848`)
- builds phase-sensitive pawn/king PST context and clears pawn hash (`src/evaluation.cpp:1786-1853`).

---

## 3 — Choosing the best next move

Iterative deepening root loop (`IterateInt`) with aspiration windows, PV updates, and root move resorting; final best move is `mvs[0]` copied to `m_BestMove` and applied in `SearchRoot`.

### 3.1 Root best-move selection loop

In `IterateInt()` (`src/search.cpp:1248`):

1. Build root legal move list once:
   - `sd->m_wRootMoves = p->LegalMoves(sd->m_hHeap)` (`src/search.cpp:1267`)
2. For each iterative depth (`m_wDepth`) and each root move (`m_wMoveNum`):
   - search move score by calling `NegaScout` or `Quies` (`src/search.cpp:1304-1324`)
   - perform aspiration-window fail-low/fail-high re-search when needed (`src/search.cpp:1328-1455`)
   - update PV info, `m_nBestScore`, and analysis lines (`src/search.cpp:1464-1491`)
3. Reorder root moves for next iteration via `ResortMovesList` (`src/search.cpp:1529`, defined at `1217`).
4. Finalize selected move:
   - `sd->m_BestMove = mvs[0]` (`src/search.cpp:1636`)

So, the “best move” is the head of the root list after iterative deepening + reordering + aspiration re-search convergence.

### 3.2 Returning and playing the chosen move

- `Iterate()` copies out `sd->m_BestMove` and `sd->m_nBestScore`, then returns it (`src/search.cpp:1777-1793`).
- `SearchRoot()` receives the move and executes it on the actual game position with `p->DoMove(move)` (`src/search.cpp:1825-1838`).

Because `DoMove()` toggles `m_nTurn` (`src/dbase.cpp:662-665`), the same pipeline computes “best move for each side” naturally on alternating turns.

  

