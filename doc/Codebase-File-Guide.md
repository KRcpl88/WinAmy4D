# WinAmy Codebase: Detailed File Guide

This document reflects the current `WinAmy4D` layout after the C-to-C++ refactor, the
addition of the floating-point lattice geometry types, and the new `WinAmyGUI` graphical
application (a Win32 + Direct3D 11 front end for the engine).

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

7. **Graphical front end (`WinAmyGUI/`)**
   - `WinAmyGUI` is a separate Win32 desktop application (its own EXE) that links the
     `src` engine library and lets a human play against, or analyze with, the engine.
   - `WinAmyGUI/main.cpp` is a thin `WinMain` that owns a single `CWinAmy4dWnd`
     (`WinAmy4dWnd.h/.cpp`), which encapsulates all window/control state and interaction.
   - `GameController` runs engine searches on a background thread and posts results back to
     the window; `BoardRenderer` (flat 2D view) and `D3DBoardRenderer` (Direct3D 11 3D
     wireframe view) draw the board, with piece glyphs supplied by `PieceAtlas`.
   - The 3D renderer consumes the floating-point lattice geometry types
     (`CUCoordFloat`, `CUCoordRotate`, `CChord`, `CPoly`) to build cell outlines and pick rays.

---

## 2) Repository file-by-file reference

### Root files

- `README.md` — primary user/build docs.
- `CHANGELOG.md` — project changelog.
- `LICENSE` — BSD-2-Clause license.
- `HOW_TO_PLAY_4D_CHESS.md` — rules/intro for the 4D chess variant.
- `WinAmy.sln` — solution wiring `src` (static lib), `WinAmy` (console exe), `WinAmyTests` (test DLL), and `WinAmyGUI` (graphical exe).
- `.clang-format` — formatting rules.
- `.gitignore` — ignore rules (including generated `include/config.h`).
- `Preferences` — opening preference lines.
- `prompts.md` — scratch/prompt notes.
- `matebug.pgn` — sample PGN used while investigating a mate-handling bug.
- `AmyLogo.gif` — repository artwork.
- `2D.png`, `4d_board.JPG`, `dodecahedron.png`, `knight.PNG`, `rook1.PNG`, `rook2.PNG` — board/piece imagery used by the docs.

### GitHub/editor configuration

- `.github/workflows/c-cpp.yml` — Release x64 CI build.
- `.github/workflows/unit-tests.yml` — Debug x64 test build + vstest run.
- `.github/workflows/release.yml` — tag-driven release pipeline.
- `.github/workflows/release-binaries.yml` — attaches built binaries to a published release.
- `.github/workflows/copilot-setup-steps.yml` — environment setup steps for the Copilot agent.
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

#### `tools/`

Helper scripts. `tools/epd_to_epd4.py` converts a standard EPD file to the 3D `EPD4`
format by prefixing level `a` to each square reference in SAN notation.

### Executable project (`WinAmy/`)

- `WinAmy/main.cpp` — production entrypoint.
- `WinAmy/WinAmy.vcxproj` — executable project; references `src`.
- `WinAmy/WinAmy.cpp` — template/legacy stub, not active entrypoint.

### Graphical application project (`WinAmyGUI/`)

A standalone Win32 + Direct3D 11 desktop EXE that links the `src` engine library.

- `WinAmyGUI/main.cpp` — `WinMain` entrypoint; owns and runs a single `CWinAmy4dWnd`.
- `WinAmyGUI/WinAmy4dWnd.h/.cpp` — `CWinAmy4dWnd`, the main window: control creation,
  message routing, click-to-move and view interaction, menu handling.
- `WinAmyGUI/GameController.h/.cpp` — `GameController`, drives engine searches on a worker
  thread, validates/apply moves, posts results to the window (includes undo support).
- `WinAmyGUI/BoardRenderer.h/.cpp` — flat 2D board renderer with selectable view plane
  (axis swap).
- `WinAmyGUI/D3DBoardRenderer.h/.cpp` — Direct3D 11 3D wireframe renderer with orbit/zoom
  and ray-pick selection.
- `WinAmyGUI/PieceAtlas.h/.cpp` — pre-renders Unicode chess glyphs into a texture atlas for
  the 3D renderer.
- `WinAmyGUI/WinAmyGUI.rc`, `WinAmyGUI/resource.h` — menus, dialogs, and resource IDs.
- `WinAmyGUI/WinAmyGUI.vcxproj` (+ `.filters`) — Windows-subsystem application project.

### Core engine project (`src/`)

- `src/src.vcxproj` — static-library project containing the engine.
- `src/framework.h` — project framework placeholder.

### Core source modules (refactored to `.cpp`)

- Startup/system: `init.cpp`, `utils.cpp`, `random.cpp`.
- Position/moves/bitboards: `dbase.cpp`, `position.cpp`, `move.cpp`, `bitboard.cpp`, `movedata.cpp`, `swap.cpp`, `scoord.cpp`, `ucoord.cpp`, `hcoord.cpp`.
- Floating-point lattice geometry (used by the GUI 3D renderer): `ucoordFloat.cpp` (`CUCoordFloat`), `ucoordRotate.cpp` (`CUCoordRotate`), `chord.cpp` (`CChord`, an edge between two points), `poly.cpp` (`CPoly`, a triangle).
- Search/time/hash: `search.cpp`, `search_data.cpp`, `search_io.cpp`, `time_ctl.cpp`, `hashtable.cpp`, `mates.cpp`, `recog.cpp`.
- Evaluation/config: `evaluation.cpp`, `evaluation_config.cpp`, `yaml.cpp`, `tree.cpp`, `heap.cpp`.
- Command/protocol loop: `state_machine.cpp`, `commands.cpp`.
- Book/analysis tools: `pgn.cpp`, `eco.cpp`, `bookup.cpp`, `learn.cpp`, `filter.cpp`, `blunder.cpp`.
- Endgame tablebase probing: `probe.cpp` (disabled stub; the original 8x8/2D tablebase index code was removed as invalid for 4D).
- Internal engine tests: `test_blunder.cpp`, `test_dbase.cpp`, `test_yaml.cpp`.
- `src/main.cpp` exists but is not the active executable entrypoint in `WinAmy.vcxproj`.

### Public headers (`include/`)

- Core types/inlines/config: `amy.h`, `types.h`, `inline.h`, `safe_malloc.h`, `config.h` (generated).
- C++ class APIs:
  - `move.h` (`CMove`)
  - `scoord.h`, `ucoord.h`, `hcoord.h` (`CSCoord`, `CUCoord`, `CHCoord`)
  - `ucoordFloat.h`, `ucoordRotate.h`, `chord.h`, `poly.h` (`CUCoordFloat`, `CUCoordRotate`, `CChord`, `CPoly` — floating-point lattice geometry)
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
  - `AttackConsistencyTests.cpp`
  - `AttackDeltaTests.cpp`
  - `MoveTests.cpp`
  - `PositionTests.cpp`
  - `CoordTests.cpp`
  - `SearchDataTests.cpp`
  - `CheckmateTests.cpp`
  - `ChordTests.cpp`
  - `PolyTests.cpp`
  - `UCoordFloatTests.cpp`
  - `UCoordRotateTests.cpp`

---

## 3) Important practical notes

- The engine codebase now compiles as C++ (`.cpp`) with C-style APIs preserved where needed.
- The `src` project is a static library shared by both the console engine (`WinAmy`) and the graphical app (`WinAmyGUI`).
- `WinAmyGUI` is a Win32 + Direct3D 11 desktop application; building it requires the Windows SDK (Direct3D 11/D2D/DWrite).
- `include/config.h` is generated (gitignored) before builds.
- `AttackConsistencyTests` contains the `EngineSelfPlayKeepsBothKings` test tagged `LongRunning`, which is excluded from the default CI test pass.
- Common local commands:
  - Build solution (Debug x64): `msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m`
  - Build tests: `msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m /t:WinAmyTests:Rebuild`
  - Run tests: `vstest.console.exe x64\Debug\WinAmyTests.dll /Platform:x64`
