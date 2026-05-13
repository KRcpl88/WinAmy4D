# WinAmy Codebase: Detailed File Guide

This document explains how the code works in detail and what each file does.

## 1) How the engine works end-to-end

1. **Process startup**
   - `/home/runner/work/WinAmy/WinAmy/WinAmy/main.c` is the executable entrypoint.
   - It initializes move tables, engine globals, hash tables, opening-book learning state, endgame-tablebase path, RNG, and optional evaluation config.
   - It then enters the interactive engine loop (`StateMachine`).

2. **Input + protocol loop**
   - `/home/runner/work/WinAmy/WinAmy/src/state_machine.c` drives high-level UI states:
     - waiting for user/xboard command,
     - calculating,
     - pondering,
     - analyzing,
     - exit.
   - `/home/runner/work/WinAmy/WinAmy/src/commands.c` parses user/xboard input into either a command handler or a legal move.

3. **Position representation + move legality**
   - Core board state, attack maps, piece masks, and move making/undoing are handled across:
     - `dbase.c`, `bitboard.c`, `movedata.c`, `next.c`, `magic.c`, `swap.c`.
   - `DoMove/UndoMove`, SAN parsing/formatting, legal move generation, and attack recomputation are foundational for search.

4. **Search + time management**
   - `/home/runner/work/WinAmy/WinAmy/src/search.c` is the main iterative deepening tree search (negascout/PVS-style with pruning/extensions).
   - `/home/runner/work/WinAmy/WinAmy/src/time_ctl.c` computes move budgets and stopping logic.
   - `/home/runner/work/WinAmy/WinAmy/src/search_io.c` formats principal variation and search stats.
   - Hash table lookups (`hashtable.c`) and recognizers/tablebases (`recog.c`, `probe.c`) improve speed and tactical/ending strength.

5. **Evaluation + tuning**
   - `/home/runner/work/WinAmy/WinAmy/src/evaluation.c` computes static score from many tuned parameters.
   - `/home/runner/work/WinAmy/WinAmy/src/evaluation_config.c` loads/saves those parameters via a YAML-like config.
   - YAML parsing and tree storage live in `yaml.c` and `tree.c`.

6. **Books, PGN/EPD tooling, testing helpers**
   - Opening-book build/flatten/learning: `bookup.c`, `learn.c`.
   - PGN + ECO + analysis tools: `pgn.c`, `eco.c`, `filter.c`, `blunder.c`.
   - Internal self-tests: `test_*.c` modules and `--test` startup option.

---

## 2) Repository file-by-file reference

### Root files

- `/home/runner/work/WinAmy/WinAmy/README.md`
  - Primary user/build documentation for Windows (VS/MSBuild, VS Code tasks, runtime flags).
- `/home/runner/work/WinAmy/WinAmy/CHANGELOG.md`
  - Human changelog/history.
- `/home/runner/work/WinAmy/WinAmy/LICENSE`
  - BSD-2-Clause license text for Amy codebase (tablebase code has separate copyright constraints).
- `/home/runner/work/WinAmy/WinAmy/WinAmy.sln`
  - Visual Studio solution wiring three projects: `WinAmy` (exe), `src` (static lib), `WinAmyTests` (test DLL).
- `/home/runner/work/WinAmy/WinAmy/.clang-format`
  - Source formatting rules.
- `/home/runner/work/WinAmy/WinAmy/.gitignore`
  - Ignore rules, including generated `config.h`, build output directories, logs, and generated DB artifacts.
- `/home/runner/work/WinAmy/WinAmy/Preferences`
  - Opening preference lines for book selection control.
- `/home/runner/work/WinAmy/WinAmy/AmyLogo.gif`
  - Repository artwork asset.

### GitHub and editor configuration

- `/home/runner/work/WinAmy/WinAmy/.github/workflows/c-cpp.yml`
  - Windows CI: checkout, setup MSBuild, generate `Include/config.h`, build solution.
- `/home/runner/work/WinAmy/WinAmy/.github/workflows/release.yml`
  - Tag-triggered release pipeline: build Release x64, package source archive, upload `x64/Release/WinAmy.exe`.
- `/home/runner/work/WinAmy/WinAmy/.github/copilot-instructions.md`
  - Repository instructions for Copilot agent behavior.
- `/home/runner/work/WinAmy/WinAmy/.vscode/tasks.json`
  - VS Code build/clean/rebuild tasks for MSBuild configurations.
- `/home/runner/work/WinAmy/WinAmy/.vscode/launch.json`
  - VS Code debug/launch profiles targeting built Debug/Release binaries.

### Documentation

- `/home/runner/work/WinAmy/WinAmy/doc/Handbook.md`
  - User and operator handbook: runtime options, command usage, test suites, book workflows.
- `/home/runner/work/WinAmy/WinAmy/doc/Amy.6`
  - Manpage-style command reference.

### Data directories

#### EPD test suites (`/home/runner/work/WinAmy/WinAmy/EPD`)

- `/home/runner/work/WinAmy/WinAmy/EPD/.gitignore`
  - Ignore rules for generated EPD outputs.
- `/home/runner/work/WinAmy/WinAmy/EPD/WAC.epd`
  - Win-at-Chess tactical benchmark set.
- `/home/runner/work/WinAmy/WinAmy/EPD/wacnew.epd`
  - Alternate/new WAC variant set.
- `/home/runner/work/WinAmy/WinAmy/EPD/bt2630.epd`
  - BT2630 benchmark positions.
- `/home/runner/work/WinAmy/WinAmy/EPD/lct2.epd`
  - LCT2 benchmark positions.
- `/home/runner/work/WinAmy/WinAmy/EPD/bs2830.epd`
  - BS2830 benchmark positions.
- `/home/runner/work/WinAmy/WinAmy/EPD/BK.epd`
  - Additional EPD benchmark/analysis set.
- `/home/runner/work/WinAmy/WinAmy/EPD/ECE3.epd`
  - Additional EPD benchmark set.
- `/home/runner/work/WinAmy/WinAmy/EPD/GMG1.epd`
  - Additional EPD benchmark set.
- `/home/runner/work/WinAmy/WinAmy/EPD/MES.epd`
  - Additional EPD benchmark set.
- `/home/runner/work/WinAmy/WinAmy/EPD/SBD.epd`
  - Additional EPD benchmark set.
- `/home/runner/work/WinAmy/WinAmy/EPD/kaufmann.epd`
  - Additional EPD benchmark set.

#### PGN data (`/home/runner/work/WinAmy/WinAmy/PGN`)

- `/home/runner/work/WinAmy/WinAmy/PGN/ClassicGames.pgn`
  - Source games used for opening-book construction/testing.
- `/home/runner/work/WinAmy/WinAmy/PGN/eco.pgn`
  - ECO reference PGN for ECO database generation.

### Executable project (`/home/runner/work/WinAmy/WinAmy/WinAmy`)

- `/home/runner/work/WinAmy/WinAmy/WinAmy/main.c`
  - **Actual entrypoint** for production binary.
  - Parses command-line options (`-ht`, `-conf`, `--test`, optional `-cpu`), processes `.amyrc`/`Amy.ini`, initializes subsystems, starts state machine.
- `/home/runner/work/WinAmy/WinAmy/WinAmy/WinAmy.vcxproj`
  - MSBuild project for executable target; links against `src` static library.
- `/home/runner/work/WinAmy/WinAmy/WinAmy/WinAmy.vcxproj.filters`
  - VS UI filter metadata.
- `/home/runner/work/WinAmy/WinAmy/WinAmy/WinAmy.vcxproj.user`
  - Per-user/local Visual Studio settings.
- `/home/runner/work/WinAmy/WinAmy/WinAmy/WinAmy.cpp`
  - Template/legacy stub file (`Hello()`); **not** part of active build in project file.
- `/home/runner/work/WinAmy/WinAmy/WinAmy/.gitignore`
  - Local ignore rules for this folder.

### Core engine project (`/home/runner/work/WinAmy/WinAmy/src`)

- `/home/runner/work/WinAmy/WinAmy/src/src.vcxproj`
  - Static-library project containing almost all engine modules.
- `/home/runner/work/WinAmy/WinAmy/src/src.vcxproj.filters`
  - VS UI filter metadata.
- `/home/runner/work/WinAmy/WinAmy/src/src.vcxproj.user`
  - Per-user/local Visual Studio settings.
- `/home/runner/work/WinAmy/WinAmy/src/framework.h`
  - Minimal framework/header placeholder in project.
- `/home/runner/work/WinAmy/WinAmy/src/.gitignore`
  - Local ignore rules for this folder.

### Core source modules

- `/home/runner/work/WinAmy/WinAmy/src/main.c`
  - Alternate/legacy engine entrypoint source (mirrors executable startup logic and test triggering).
- `/home/runner/work/WinAmy/WinAmy/src/init.c`
  - Global initialization routines (precomputed tables and engine startup data).
- `/home/runner/work/WinAmy/WinAmy/src/utils.c`
  - Generic utility helpers (I/O wrappers, tokenization, logging, timing helpers, misc support).
- `/home/runner/work/WinAmy/WinAmy/src/random.c`
  - 64-bit Mersenne Twister RNG implementation used by engine randomness/Zobrist-related initialization flows.

### Position, bitboards, and moves

- `/home/runner/work/WinAmy/WinAmy/src/dbase.c`
  - Core position database routines: position allocation/cloning/freeing, move make/undo integration, attack recalculation, board state maintenance.
- `/home/runner/work/WinAmy/WinAmy/src/bitboard.c`
  - Bitboard utilities: bit scans, masks, attack-map support primitives.
- `/home/runner/work/WinAmy/WinAmy/src/movedata.c`
  - Precomputes move/attack lookup data for piece movement.
- `/home/runner/work/WinAmy/WinAmy/src/magic.c`
  - Builds/uses magic-bitboard tables for fast sliding-piece attacks.
- `/home/runner/work/WinAmy/WinAmy/src/next.c`
  - Move generation/ordering pipeline for search (legal moves, captures/checks ordering support).
- `/home/runner/work/WinAmy/WinAmy/src/swap.c`
  - Static exchange evaluation (SEE) logic for tactical capture quality assessment.

### Search and time control

- `/home/runner/work/WinAmy/WinAmy/src/search.c`
  - Main tree search implementation (iterative deepening, quiescence, pruning/reductions/extensions, pondering behavior, stop/abort checks).
- `/home/runner/work/WinAmy/WinAmy/src/search_io.c`
  - Search output formatting (PV lines, node/time reporting, protocol-consumable info text).
- `/home/runner/work/WinAmy/WinAmy/src/time_ctl.c`
  - Time-control parser and move-time budgeting logic.
- `/home/runner/work/WinAmy/WinAmy/src/hashtable.c`
  - Transposition table allocation/probing/storage and related hash management.
- `/home/runner/work/WinAmy/WinAmy/src/mates.c`
  - Mate-threat/pattern detection support used by search/evaluation shortcuts.
- `/home/runner/work/WinAmy/WinAmy/src/recog.c`
  - Position recognizers (interior node recognition) for known classes of positions.

### Evaluation and config

- `/home/runner/work/WinAmy/WinAmy/src/evaluation.c`
  - Static evaluation function and tuned parameters (piece-square effects, pawn structure, king safety, game phase terms, etc.).
- `/home/runner/work/WinAmy/WinAmy/src/evaluation_config.c`
  - Load/save and apply evaluation/search tunables from YAML configuration data.

### Command loop and protocol

- `/home/runner/work/WinAmy/WinAmy/src/state_machine.c`
  - Runtime control loop for interactive/xboard operation.
- `/home/runner/work/WinAmy/WinAmy/src/commands.c`
  - Command table and handlers (`new`, `go`, `force`, `bookup`, `test`, `conf`, `xboard`, etc.).

### PGN/ECO/book and analysis tooling

- `/home/runner/work/WinAmy/WinAmy/src/pgn.c`
  - PGN parsing/printing utilities for game IO.
- `/home/runner/work/WinAmy/WinAmy/src/eco.c`
  - ECO database creation and ECO code lookup utilities.
- `/home/runner/work/WinAmy/WinAmy/src/bookup.c`
  - Opening-book build/update/flatten operations.
- `/home/runner/work/WinAmy/WinAmy/src/learn.c`
  - Book learning updates from autosaved game outcomes.
- `/home/runner/work/WinAmy/WinAmy/src/filter.c`
  - Offline PGN filtering for quiescent positions using static/quiescence/full-search score deltas.
- `/home/runner/work/WinAmy/WinAmy/src/blunder.c`
  - Offline blunder analysis helper that compares annotated moves with engine-best alternatives.

### Tablebase support

- `/home/runner/work/WinAmy/WinAmy/src/probe.c`
  - Engine-side EGTB probing interface used during search/evaluation.
- `/home/runner/work/WinAmy/WinAmy/src/mytb.cpp`
  - Glue/compile unit that configures and includes tablebase indexing implementation.
- `/home/runner/work/WinAmy/WinAmy/src/tbindex.cpp`
  - Large tablebase index/probe implementation (piece indexing, symmetry transforms, compressed table access machinery).
- `/home/runner/work/WinAmy/WinAmy/src/tbdecode.c`
  - Legacy decompression primitives used by tablebase data handling.

### Generic data structures and parsing

- `/home/runner/work/WinAmy/WinAmy/src/heap.c`
  - Custom move heap/section allocator used by move-generation/search helpers.
- `/home/runner/work/WinAmy/WinAmy/src/tree.c`
  - AVL-like binary tree map with serialization helpers for key/value storage.
- `/home/runner/work/WinAmy/WinAmy/src/yaml.c`
  - Lightweight YAML-like tokenizer/parser and lookup helpers backing config loading.

### Internal C tests in `src`

- `/home/runner/work/WinAmy/WinAmy/src/test_blunder.c`
  - Unit-like checks around blunder-analysis parsing/helpers.
- `/home/runner/work/WinAmy/WinAmy/src/test_dbase.c`
  - Unit-like checks for core position/database operations.
- `/home/runner/work/WinAmy/WinAmy/src/test_yaml.c`
  - Unit-like checks for YAML parser and typed lookup helpers.

### Public headers (`/home/runner/work/WinAmy/WinAmy/include`)

> Most headers are module interfaces matching same-named `src/*.c` files.

- `/home/runner/work/WinAmy/WinAmy/include/amy.h` — global compile-time constants/macros/platform toggles.
- `/home/runner/work/WinAmy/WinAmy/include/types.h` — central type definitions (pieces, moves, bitboards, structs).
- `/home/runner/work/WinAmy/WinAmy/include/inline.h` — performance-critical inline helpers/macros.
- `/home/runner/work/WinAmy/WinAmy/include/safe_malloc.h` — checked allocation API (`safe_malloc`, `safe_realloc`, etc.).

- `/home/runner/work/WinAmy/WinAmy/include/init.h` — initialization function declarations.
- `/home/runner/work/WinAmy/WinAmy/include/utils.h` — utility function declarations.
- `/home/runner/work/WinAmy/WinAmy/include/random.h` — RNG API.

- `/home/runner/work/WinAmy/WinAmy/include/dbase.h` — position/board database API.
- `/home/runner/work/WinAmy/WinAmy/include/bitboard.h` — bitboard helpers.
- `/home/runner/work/WinAmy/WinAmy/include/movedata.h` — precomputed move-data API.
- `/home/runner/work/WinAmy/WinAmy/include/magic.h` — magic-bitboard attack APIs.
- `/home/runner/work/WinAmy/WinAmy/include/next.h` — move generation/selection API.
- `/home/runner/work/WinAmy/WinAmy/include/swap.h` — static exchange evaluation API.

- `/home/runner/work/WinAmy/WinAmy/include/search.h` — core search API and shared search globals.
- `/home/runner/work/WinAmy/WinAmy/include/search_io.h` — search output/format helpers.
- `/home/runner/work/WinAmy/WinAmy/include/time_ctl.h` — time-control parsing and runtime control API.
- `/home/runner/work/WinAmy/WinAmy/include/hashtable.h` — transposition-table API.
- `/home/runner/work/WinAmy/WinAmy/include/mates.h` — mate-threat utilities.
- `/home/runner/work/WinAmy/WinAmy/include/recog.h` — position recognizer API.
- `/home/runner/work/WinAmy/WinAmy/include/probe.h` — endgame tablebase probe API.

- `/home/runner/work/WinAmy/WinAmy/include/evaluation.h` — static evaluation API.
- `/home/runner/work/WinAmy/WinAmy/include/evaluation_config.h` — config load/save/tunable mapping API.

- `/home/runner/work/WinAmy/WinAmy/include/commands.h` — command parser/command execution interfaces.
- `/home/runner/work/WinAmy/WinAmy/include/state_machine.h` — runtime state machine interface and shared state flags.

- `/home/runner/work/WinAmy/WinAmy/include/pgn.h` — PGN parser/printer API.
- `/home/runner/work/WinAmy/WinAmy/include/eco.h` — ECO processing API.
- `/home/runner/work/WinAmy/WinAmy/include/bookup.h` — opening-book update/flatten API.
- `/home/runner/work/WinAmy/WinAmy/include/learn.h` — book-learning API.
- `/home/runner/work/WinAmy/WinAmy/include/filter.h` — quiescent-position filter API.
- `/home/runner/work/WinAmy/WinAmy/include/blunder.h` — blunder-analysis API.

- `/home/runner/work/WinAmy/WinAmy/include/tree.h` — tree container API used by YAML/config.
- `/home/runner/work/WinAmy/WinAmy/include/yaml.h` — YAML token/node model and parser/lookups.
- `/home/runner/work/WinAmy/WinAmy/include/heap.h` — custom heap structure API.

- `/home/runner/work/WinAmy/WinAmy/include/test_yaml.h` — internal YAML test entrypoints.
- `/home/runner/work/WinAmy/WinAmy/include/test_dbase.h` — internal dbase test entrypoints.
- `/home/runner/work/WinAmy/WinAmy/include/test_blunder.h` — internal blunder test entrypoints.

### Visual Studio test project (`/home/runner/work/WinAmy/WinAmy/WinAmyTests`)

- `/home/runner/work/WinAmy/WinAmy/WinAmyTests/WinAmyTests.vcxproj`
  - Test DLL project using `CppUnitTestFramework`, references engine `src` static library.
- `/home/runner/work/WinAmy/WinAmy/WinAmyTests/WinAmyTests.vcxproj.filters`
  - VS UI filter metadata.
- `/home/runner/work/WinAmy/WinAmy/WinAmyTests/WinAmyTests.cpp`
  - C++ unit tests for core bitboard/move/position invariants and attack-generation correctness.

---

## 3) Important practical notes

- Build system is Windows-first MSBuild/Visual Studio.
- CI and release workflows generate `/home/runner/work/WinAmy/WinAmy/include/config.h` before compilation (it is not tracked in git).
- Main executable output path for x64 builds is under:
  - `/home/runner/work/WinAmy/WinAmy/x64/Debug/WinAmy.exe`
  - `/home/runner/work/WinAmy/WinAmy/x64/Release/WinAmy.exe`
