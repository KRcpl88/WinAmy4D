![main branch](https://github.com/thgreiner/amy/actions/workflows/c-cpp.yml/badge.svg)
![License](https://img.shields.io/github/license/thgreiner/amy)

> 🆕 **New to 4D chess?** See **[HOW_TO_PLAY_4D_CHESS.md](HOW_TO_PLAY_4D_CHESS.md)** for a non-technical guide to the rules, the board, and how each piece moves.

What is Amy?
============

Amy is a chess playing program.  It is compatible with xboard, uses endgame
table bases and an opening book.

Copyright
=========

Amy is distributed under the BSD 2-Clause License. You should find a file
'LICENSE' in the Amy distribution containing this license.

Please note that Amy uses table base code developed and copyrighted by
Eugene Nalimov. This code is *not* under the BSD License.

Building Amy
============

Amy is built on Windows using Visual Studio 2022 (v143 toolset) or the
MSBuild command line.

### Using Visual Studio

Open `WinAmy.sln` in Visual Studio 2022 and build the solution. The default
configuration is **Debug | x64**.

### Using MSBuild from the command line

Open a **Developer Command Prompt for VS 2022** (or run `vcvarsall.bat x64`)
and run:

Build (Debug x64):

	msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m

Build (Release x64):

	msbuild WinAmy.sln /p:Configuration=Release /p:Platform=x64 /m

Clean:

	msbuild WinAmy.sln /t:Clean /p:Configuration=Debug /p:Platform=x64

Rebuild (clean + build, Debug x64):

	msbuild WinAmy.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m

The output executable is placed in `x64\Debug\WinAmy.exe` or
`x64\Release\WinAmy.exe`.

### Using VS Code

The repository includes VS Code task and launch configurations in the
`.vscode` directory. To build, press **Ctrl+Shift+B** (the default build task
is **Build WinAmy (Debug x64)**). Additional tasks are available for Release
builds, Clean, and Rebuild.

To debug or launch the program, open the **Run and Debug** panel (Ctrl+Shift+D)
and select one of the available configurations:

- **Debug WinAmy (Debug x64)** – builds and launches the Debug binary under the debugger
- **Debug WinAmy (Release x64)** – builds and launches the Release binary under the debugger
- **Launch WinAmy (Debug x64)** – launches an existing Debug binary without rebuilding
- **Launch WinAmy (Release x64)** – launches an existing Release binary without rebuilding

> **Prerequisites:** The [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
> for VS Code must be installed, and `msbuild` must be on your PATH (e.g. via
> the VS 2022 Developer Command Prompt or by launching VS Code from one).


Invoking Amy
============

Just type 'Amy'. You can also specify a hashtable size:

	WinAmy -ht 10m
	
will use 10 MB of hashtables. If you build a parallel version, you can supply
the number of processors (or threads rather) Amy should use:

	WinAmy -ht 10m -cpu 2

Note that you can specify these options via an '.amyrc' file, too. See below.
	
 

Creating and using opening books
================================

To create a book from a PGN file, first create the ECO database. At Amy's
prompt, type 'eco PGN/eco.pgn'. Verify that it works: Type 

	new
	force
	e4
	e

the output should be 

	Eco code is B00 King's pawn opening

You can now use the 'bookup' command to create an opening book from a PGN file:

	bookup ClassicGames.pgn

This command will create an opening book from the file "ClassicGames.pgn".

Since book files tend to become very large, you can make them smaller by using
the 'flatten' command. Typing

	flatten 1

will create a file 'Book2.db' which contains all positions from 'Book.db' which
occured more than one time. This typically reduces book size to 1/10th! To use
the new book, simply rename it to 'Book.db' and restart Amy.


Setting opening book preferences
================================

You want Amy to play King's gambit no longer? No problem! Just create a file
containing the lines

	e4 e5 f4?

and use 

	prefs filename

to read it in. Amy comes complete with a file 'Preferences' which avoids some
opening traps and known bad lines. Just use

	prefs Preferences

to set these.


Using book learning
===================

Starting with version 0.8.4 Amy features a simple version of book learning.
It uses the autosave files created by Amy to update the opening book statistics.
For this to work, game autosaving must be activated (see section "Setting 
options via .amyrc").


Using endgame tablebases
========================

Create a directory called 'TB' in the distribution directory. Put your 
tablebase files there. They will be recognized automagically. See also the
section on '.amyrc' below.


Setting options via .amyrc / Amy.ini
====================================

Amy supports an .amyrc file to set several options. The .amyrc file should be
in the current directory when starting amy. Here is a sample .amyrc:

```
#
# Sample .amyrc 
#
# Use 20 MB of hashtables:
ht=20m
#
# Look for tablebases on /space/TB
tbpath=/space/TB
#
# Use 2 processors for parallel search
cpu=2
#
# Enable game autosaving to allow booklearning
autosave=true
```

Since people using Windows have reported that they have to resort to DOS mode
for creating a .amyrc file, Amy also looks for Amy.ini.


THANKS
======

- to Allen Lake for tuning Amy's timing algorithm

