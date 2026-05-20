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

`GenTo` and `GenFrom` are `CPosition` methods (declared in `include/dbase.h`, implemented in `src/dbase.cpp:950` and `:988`) that convert attack/mask information into move objects. The underlying movement geometry is computed earlier in the engine initialization and attack-update layers:

- Knight/king geometry comes from precomputed lookup tables `KnightEPM[CSCoord::SIZE]` and `KingEPM[CSCoord::SIZE]` (`src/movedata.cpp:45`, `112`).
- Sliding ray geometry (line-of-sight relationships) comes from `InitAll()` → `InitGeometry()`, which builds `BishopEPM`, `RookEPM`, `QueenEPM`, `InterPath`, and `Ray` (`src/init.cpp:392-398`, `235-279`).
- Blocker-aware sliding attacks are resolved at runtime by magic helpers `bishop_attacks` / `rook_attacks` (`include/magic.h:60-75`, tables initialized in `src/magic.cpp:237-288`).
- `AtkSet(...)` chooses the piece attack model for the current board occupancy and writes results into `m_rgAtkTo` / `m_rgAtkFr` (`src/dbase.cpp:185-221`), then `GenTo`/`GenFrom`/`LegalMoves` enumerate from those maps.

#### How `bishop_attacks` / `rook_attacks` work for a specific `CPosition`

For a bishop/rook on square `sq` in a concrete position `p`:

1. `AtkSet` computes `occupied = p->m_rgMask[0][0] | p->m_rgMask[1][0]` and calls `bishop_attacks(sq, occupied)` or `rook_attacks(sq, occupied)` (`src/dbase.cpp:197-200`).
2. The helper first keeps only relevant line blockers with `occupied & <piece>_blocker_mask[sq]` (`include/magic.h:61-62`, `71-72`).
3. It hashes that blocker subset with square-specific magic multiplier and shift:
   - `(blockers * rook_magics[sq]) >> (CSCoord::SIZE - rook_index_bits[sq])`
   - `(blockers * bishop_magics[sq]) >> (CSCoord::SIZE - bishop_index_bits[sq])`  
   (`include/magic.h:62-64`, `72-74`)
4. The hash indexes prebuilt attack tables (`rook_table`, `bishop_table`) and returns the exact reachable ray squares up to first blocker (`include/magic.h:64`, `74`).
5. `AtkSet` stores that result in `m_rgAtkTo[sq]` and updates reverse map `m_rgAtkFr[*]` (`src/dbase.cpp:215-220`).
6. Move generators then apply occupancy/side constraints:
   - non-captures: `m_rgAtkTo[from] & empty` (`src/dbase.cpp:993-999`)
   - captures: target-driven from `m_rgAtkFr[to]` (`src/dbase.cpp:950-967`)
   - strict legality: `DoMove` + `!InCheck` + `UndoMove` in `legal_moves_internal` (`src/dbase.cpp:2091-2096`, `2116-2120`).

`InitAll()`-generated EPM tables (`BishopEPM`/`RookEPM`/`QueenEPM`) are not used directly inside `bishop_attacks` / `rook_attacks`; those helpers use magic tables. EPM tables are the geometry layer used elsewhere (e.g., `InterPath`/`Ray` line tests and check-detection prefilters in `LegalMove`/`IsCheckingMove`) (`src/dbase.cpp:1225-1239`, `1266-1367`).

#### Deep dive: How the magic multiplier works (step 3 above)

##### 1. Inputs to the magic multiplier

The multiplication takes two 64-bit operands:

- **`blockers`** — a bitboard containing only the occupied squares that lie along the sliding piece's attack rays from square `sq`, excluding the edges of the board. This is obtained by masking the full board occupancy with a precomputed `rook_blocker_mask[sq]` or `bishop_blocker_mask[sq]`. Example: for a rook on e4 with pieces on e2, e7, b4, and g4, the blocker bitboard would have bits set at those four positions.

- **`<piece>_magics[sq]`** — a square-specific 64-bit constant (the "magic number") chosen so that when multiplied by any possible blocker configuration for that square, the product's upper bits form a unique index distinguishing each distinct blocker pattern. These constants were found by offline trial-and-error search and are hardcoded in `src/magic.cpp`.

##### 2. How the multiplication result is used

The 64-bit product `blockers * magic[sq]` is right-shifted by `(64 - index_bits[sq])` to extract only the top N bits, where N = `index_bits[sq]` (typically 10–12 for rooks, 5–9 for bishops depending on the square). This produces a small integer in the range `[0, 2^N)` — the **magic index**.

The magic index is then used to look up the prebuilt attack table:
```
rook_table[rook_table_offsets[sq] + magic_index]
bishop_table[bishop_table_offsets[sq] + magic_index]
```

The value stored at that offset is the exact bitboard of all squares the sliding piece can reach from `sq` given that specific blocker configuration — i.e., rays extending in each direction up to and including the first blocker.

##### 3. Purpose and function of the magic multiplier

The magic multiplier is a **perfect hash function** for blocker configurations. Its purpose is to convert a sparse, high-entropy 64-bit blocker pattern into a compact, dense index suitable for table lookup — all in a single multiplication + shift (2 CPU instructions).

Without magic multiplication, determining which squares a rook or bishop can reach requires iterating along each ray direction, checking for blockers at each step. That loop-based approach has variable cost. The magic technique replaces it with constant-time O(1) lookup: one AND (mask), one MULTIPLY, one SHIFT, one table read.

The "magic" property is that the chosen constant scatters different blocker configurations into different index slots with no collisions. Finding such constants is computationally expensive (done offline), but using them at runtime is trivially cheap.

##### 4. Purpose of the hash-indexed prebuilt attack tables

The tables `rook_table[102400]` and `bishop_table[5248]` (`src/magic.cpp:43-45`) store precomputed attack bitboards for every (square, blocker-configuration) pair. Each entry is a full 64-bit bitboard showing exactly which squares the piece attacks given those blockers.

At initialization time (`InitMagic()` → `init_rook_table()` / `init_bishop_table()`), the engine:
1. Enumerates every possible blocker subset for each square (2^N subsets where N = number of relevant blocker squares).
2. For each subset, computes the actual attack bitboard by ray-walking (`rook_attack_mask()` / `bishop_attack_mask()` in `src/magic.cpp:152-223`).
3. Hashes the blocker subset using the magic multiplier to get the index.
4. Stores the attack bitboard at that index in the table.

At runtime, the same hash operation reproduces the same index, and the table returns the precomputed answer instantly.

##### 5. Where are the tables computed?

- **`InitMagic()`** (`src/magic.cpp:285-288`) is called during engine startup from `InitAll()`.
- **`init_rook_table()`** (`src/magic.cpp:237-254`) populates `rook_table[]` — iterates all 64 squares, generates all blocker subsets, computes attack masks via `rook_attack_mask()`, and stores them at magic-indexed positions.
- **`init_bishop_table()`** (`src/magic.cpp:259-280`) does the same for `bishop_table[]` using `bishop_attack_mask()`.
- **`rook_attack_mask(sq, blockers)`** (`src/magic.cpp:152-189`) computes the actual attack set by walking left/right/up/down rays from `sq`, stopping at blockers.
- **`bishop_attack_mask(sq, blockers)`** (`src/magic.cpp:191-223`) does the same for diagonal rays.
- The magic constants themselves (`rook_magics[]`, `bishop_magics[]`) and blocker masks (`rook_blocker_mask[]`, `bishop_blocker_mask[]`) are hardcoded arrays found by offline search (`src/magic.cpp:47-148`).

##### 6. Do magic multipliers assume an 8×8, 64-square board?

**Yes, fundamentally.** The magic bitboard technique is deeply tied to the 64-square / 64-bit assumption:

- Each blocker mask, magic constant, and index-bits value is specific to one of exactly 64 squares on an 8×8 board.
- The technique exploits the fact that the entire board state fits in a single 64-bit integer, making the multiplication a single CPU instruction operating on the complete positional information.
- The hardcoded magic constants were found by brute-force search specifically for the geometry of an 8×8 board — the number of relevant blocker squares per ray, edge exclusions, and shift amounts all assume 8 ranks and 8 files.
- With more than 64 squares, the board state no longer fits in a single `uint64_t`, so the fundamental premise (one multiply on one machine word covers the whole board) breaks down.
- The blocker mask arrays, magic constants, and prebuilt tables would all need to be regenerated for any different board geometry, and for boards larger than 64 squares, the technique cannot work with standard 64-bit multiplication at all.

##### 7. Alternative for multi-level/non-8×8 boards: ray-walk computation

For WinAmy4D's 3D multi-level board (344 squares across levels of varying size), magic bitboards are **not viable**. However, the same functional goal — "given a sliding piece on square S and a set of occupied squares, compute which squares the piece can reach" — can be achieved with direct ray-walking logic:

```cpp
CBitBoard ComputeRookAttacks(CSCoord sq, const CBitBoard& occupied) {
    CBitBoard attacks;
    // Walk each orthogonal ray direction until blocked or off-board
    for (each direction dir in {+file, -file, +rank, -rank, +level, -level}) {
        CSCoord current = sq;
        while (true) {
            current = current.Step(dir);  // move one step in direction
            if (!current.IsValid()) break;  // off board edge
            attacks.SetBit(current.BitOffset());
            if (occupied.TstBit(current.BitOffset())) break;  // hit a blocker
        }
    }
    return attacks;
}
```

This is essentially what `rook_attack_mask()` and `bishop_attack_mask()` already do in `src/magic.cpp:152-223` (they use `ShiftLeft`/`ShiftRight`/`ShiftUp`/`ShiftDown` ray walks) — but currently only at initialization time to populate the lookup tables. For a 3D board, we would use this ray-walk approach at **runtime** instead of table lookup.

**Trade-offs:**
- **Performance**: Ray-walking is O(ray_length) per direction rather than O(1). For a typical piece with 4–8 ray directions and average ray length of 3–5 squares, this means ~15–40 operations per attack computation vs. 4 operations (AND, MUL, SHIFT, LOAD) for magic. In practice this is still fast enough for a chess engine — many strong engines used this approach before magic bitboards were invented.
- **Flexibility**: Ray-walking works for any board topology — rectangular, 3D, hexagonal, or irregular. The only requirement is a `Step(direction)` function that knows the board's adjacency graph.
- **Memory**: Eliminates ~840 KB of precomputed tables (`rook_table` + `bishop_table`), plus per-square magic constants.
- **Correctness**: The `bishop_attack_mask` and `rook_attack_mask` functions in `src/magic.cpp` already implement this logic correctly; they just need to be promoted from init-time helpers to runtime functions operating on CSCoord and the multi-level board geometry.

**Implementation path for WinAmy4D:**
1. Add directional step logic to CSCoord (e.g., `CSCoord::Step(Direction)` returning an optional/invalid CSCoord when stepping off-board or off-level).
2. Implement runtime `ComputeBishopAttacks(CSCoord, CBitBoard)` and `ComputeRookAttacks(CSCoord, CBitBoard)` using ray-walk loops.
3. For the 3D board, add new ray directions for inter-level movement (if pieces can move between levels).
4. Replace `bishop_attacks(sq, occupied)` / `rook_attacks(sq, occupied)` calls in `AtkSet` with the new runtime functions.
5. The magic tables, blocker masks, and magic constants can then be removed entirely.

**Key insight:** The actual attack-set answer is already computed by simple ray-walking logic that exists in the codebase (`rook_attack_mask` / `bishop_attack_mask` in `src/magic.cpp:152-223`). For the 3D board transition, we simply promote that ray-walk from init-time to runtime and add new directional steps for the expanded geometry. Everything downstream — `m_rgAtkTo`/`m_rgAtkFr` population, `GenTo`/`GenFrom`, legality filtering — stays exactly the same, since it only consumes the resulting attack bitboards regardless of how they were computed.

#### How queen legal moves are generated for a specific `CPosition`

Yes. For queen attack generation on a specific board, `AtkSet` computes the union of magic bishop and magic rook attacks on the same occupancy:

`bishop_attacks(square, occupied) | rook_attacks(square, occupied)` (`src/dbase.cpp:202-205`).

That union is then consumed by normal generation/legality filters (`GenTo`/`GenFrom`, then `DoMove`/`InCheck`/`UndoMove` in strict legal generation), so final queen moves are legal moves, not just raw attack squares.

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

  

