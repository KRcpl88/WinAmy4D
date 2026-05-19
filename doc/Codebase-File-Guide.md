# WinAmy Codebase: Detailed File Guide

This document reflects the current `WinAmy4D` layout after the C-to-C++ refactor and file renames.

## 1) How the engine works end-to-end

1. **Process startup**
   - `WinAmy/main.cpp` is the executable entrypoint.
   - It initializes move tables, engine globals, hash tables, book learning, EGTB path, RNG, and optional evaluation config.
   - It enters the interactive loop via `StateMachine()`.

2. **Input + protocol loop**
   - `src/state_machine.cpp` drives runtime states (waiting, calculating, pondering, analyzing, exit).
   - `src/commands.cpp` parses command/xboard input into handlers or legal moves.

3. **Position + move legality**
   - Core state lives in `CPosition` (`include/dbase.h`, implemented across `src/dbase.cpp` + `src/position.cpp`).
   - Move value object is `CMove` (`include/move.h`, `src/move.cpp`).
   - Coordinate wrappers are `CSCoord`, `CUCoord`, `CHCoord` (`include/scoord.h`, `include/ucoord.h`, `include/hcoord.h` and matching `src/*.cpp` files).
   - Bit operations are in `CBitBoard` (`include/bitboard.h`, `src/bitboard.cpp`).

4. **Search + time management**
   - Search is split across:
     - `src/search.cpp` (core tree search),
     - `src/search_data.cpp` (`CSearchData` move ordering/state),
     - `src/position.cpp` (`CPosition` search entrypoints such as `Iterate`, `SearchRoot`, `AnalysisMode`, `PermanentBrain`, `QuiescenceSearch`).
   - `src/time_ctl.cpp` handles time budgeting and stop logic.
   - `src/search_io.cpp` prints PV/search stats.

5. **Evaluation + config**
   - `src/evaluation.cpp` computes static evaluation.
   - `src/evaluation_config.cpp` loads/saves tunables.
   - Config parsing/data structures are in `src/yaml.cpp` and `src/tree.cpp`.

6. **Book/PGN/EPD tooling and tests**
   - Book + learning: `src/bookup.cpp`, `src/learn.cpp`.
   - PGN/ECO + analysis tools: `src/pgn.cpp`, `src/eco.cpp`, `src/filter.cpp`, `src/blunder.cpp`.
   - Internal C-style self-tests in engine project: `src/test_*.cpp`.
   - CppUnit tests are in `WinAmyTests/*.cpp`.

---

## 2) Repository file-by-file reference

### Root files

- `README.md` — primary user/build docs.
- `CHANGELOG.md` — project changelog.
- `LICENSE` — BSD-2-Clause license.
- `WinAmy.sln` — solution wiring `src` (static lib), `WinAmy` (exe), `WinAmyTests` (test DLL).
- `.clang-format` — formatting rules.
- `.gitignore` — ignore rules (including generated `include/config.h`).
- `Preferences` — opening preference lines.
- `AmyLogo.gif` — repository artwork.

### GitHub/editor configuration

- `.github/workflows/c-cpp.yml` — Release x64 CI build.
- `.github/workflows/unit-tests.yml` — Debug x64 test build + vstest run.
- `.github/workflows/release.yml` — tag-driven release pipeline.
- `.github/copilot-instructions.md` — repository Copilot instructions.
- `.vscode/tasks.json` / `.vscode/launch.json` — local build/debug tasks.

### Documentation (`doc/`)

- `doc/Handbook.md` — user/operator handbook.
- `doc/Amy.6` — manpage-style command reference.
- `doc/Codebase-File-Guide.md` — this file.
- `doc/Move-Computation-Design.md` — move computation design notes.

### Data directories

#### `EPD/`

Benchmark/test EPD suites used by testing and analysis tooling.

#### `PGN/`

PGN data used for book/ECO workflows.

### Executable project (`WinAmy/`)

- `WinAmy/main.cpp` — production entrypoint.
- `WinAmy/WinAmy.vcxproj` — executable project; references `src`.
- `WinAmy/WinAmy.cpp` — template/legacy stub, not active entrypoint.

### Core engine project (`src/`)

- `src/src.vcxproj` — static-library project containing the engine.
- `src/framework.h` — project framework placeholder.

### Core source modules (refactored to `.cpp`)

- Startup/system: `init.cpp`, `utils.cpp`, `random.cpp`.
- Position/moves/bitboards: `dbase.cpp`, `position.cpp`, `move.cpp`, `bitboard.cpp`, `movedata.cpp`, `magic.cpp`, `swap.cpp`, `scoord.cpp`, `ucoord.cpp`, `hcoord.cpp`.
- Search/time/hash: `search.cpp`, `search_data.cpp`, `search_io.cpp`, `time_ctl.cpp`, `hashtable.cpp`, `mates.cpp`, `recog.cpp`.
- Evaluation/config: `evaluation.cpp`, `evaluation_config.cpp`, `yaml.cpp`, `tree.cpp`, `heap.cpp`.
- Command/protocol loop: `state_machine.cpp`, `commands.cpp`.
- Book/analysis tools: `pgn.cpp`, `eco.cpp`, `bookup.cpp`, `learn.cpp`, `filter.cpp`, `blunder.cpp`.
- Tablebase support: `probe.cpp`, `mytb.cpp`, `tbindex.cpp`, `tbdecode.c`.
- Internal engine tests: `test_blunder.cpp`, `test_dbase.cpp`, `test_yaml.cpp`.
- `src/main.cpp` exists but is not the active executable entrypoint in `WinAmy.vcxproj`.

### Public headers (`include/`)

- Core types/inlines/config: `amy.h`, `types.h`, `inline.h`, `safe_malloc.h`, `config.h` (generated).
- C++ class APIs:
  - `move.h` (`CMove`)
  - `scoord.h`, `ucoord.h`, `hcoord.h` (`CSCoord`, `CUCoord`, `CHCoord`)
  - `bitboard.h` (`CBitBoard`)
  - `dbase.h` (`CPosition`)
  - `searchdata.h` (`CSearchData` and search status/phase structs)
- Other module interfaces remain in matching headers (`*.h`) for the corresponding `src/*.cpp` modules.

### Visual Studio test project (`WinAmyTests/`)

- `WinAmyTests/WinAmyTests.vcxproj` — CppUnit test DLL project referencing `src`.
- `WinAmyTests/TestHelpers.h`, `WinAmyTests/TestHelpers.cpp` — shared test helpers.
- Test classes:
  - `BitboardTests.cpp`
  - `AttackTests.cpp`
  - `MoveTests.cpp`
  - `PositionTests.cpp`
  - `CoordTests.cpp`
  - `SearchDataTests.cpp`

---

## 3) Important practical notes

- The engine codebase now compiles as C++ (`.cpp`) with C-style APIs preserved where needed.
- `include/config.h` is generated (gitignored) before builds.
- Common local commands:
  - Build solution (Debug x64): `msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m`
  - Build tests: `msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m /t:WinAmyTests:Rebuild`
  - Run tests: `vstest.console.exe x64\Debug\WinAmyTests.dll /Platform:x64`
