# Setup

**Last updated:** 2026-11-10 (all dependencies, split into Unreal-only and Visual Studio)

Things you need to install or do yourself after cloning. Everything else comes with the repo.

## Install

The repo has no compiled code (`Binaries/` is ignored), so the editor compiles the C++ module the first time it opens the project. That means **a C++ compiler is needed even if you only work in Unreal**. Part A is enough for that. Part B is only for writing and debugging C++ in Visual Studio.

### A. Needed to open the project in Unreal Engine 5.8

- **Unreal Engine 5.8** (made with 5.8.3, also works with 5.8.4), from the Epic Games Launcher.
- **The Aura plugin**: the project has it turned on, so the editor won't open the project without it. On fab.com (signed in with the Epic account you use in the launcher), add Aura to **My Library**. Then in the Epic Games Launcher, go to **Library → Fab Library**, find Aura and click **Install to Engine** (5.8). It installs into the engine (`Engine/Plugins/Marketplace/Aura`), not the project. Nothing in the game uses it, so if you can't get it, set Aura's `"Enabled"` to `false` in `ZSurvivalGame.uproject` on that computer, and don't commit that change.
- **The Fab plugin fix**: if C++ builds fail with "Expecting to find a type ... named 'FabToolset'", apply the fix in NOTES.md → Known issues. It's a change to the engine's files, so every computer needs it once.
- **Git**, with **Git LFS** (Large File Storage). The Git for Windows installer includes it.
- **A C++ compiler and Windows SDK**, from the Visual Studio Installer. Visual Studio 2022 (17.8 or newer) or 2026 both work. Install these, under **Individual components**:

  | What | Visual Studio 2026 | Visual Studio 2022 |
  |---|---|---|
  | MSVC compiler | **MSVC v14.50 (18.0 LTSC) – x64/x86** (`VC.14.50.18.0.x86.x64`) | **MSVC v14.44 (17.14 LTSC) – x64/x86** (`VC.14.44.17.14.x86.x64`) |
  | ATL for the same MSVC | `VC.14.50.18.0.ATL` | `VC.14.44.17.14.ATL` |
  | Latest MSVC build tools | `VC.Tools.x86.x64` | `VC.Tools.x86.x64` |
  | Windows SDK | **Windows 11 SDK (10.0.22621)** | same |
  | .NET Framework | **.NET Framework 4.6.2 targeting pack** | same |

  The engine refuses MSVC 14.50 versions below 14.50.35723 and 14.44 versions below 14.44.35211 (they have compiler bugs). Up-to-date installer versions are above those. The engine's own list of supported versions is in `Engine/Config/Windows/Windows_SDK.json`.
- **No .NET SDK to install**: the engine brings its own .NET 10 SDK for its build tools ("Using bundled DotNet SDK version: 10.0" in the build log).

### B. Also needed to work in Visual Studio

- **Workloads:** **Game development with C++**, **Desktop development with C++** and **.NET desktop development**.
- **Individual components:** **Unreal Engine integration** (IDE support, `Component.Unreal.Ide`), **Unreal Engine debugger** (`Component.Unreal.Debugger`) and **C++ Clang compiler for Windows** (`VC.Llvm.Clang`).
- **Shortcut for A and B:** in the Visual Studio Installer, click **More → Import configuration** and choose `.vsconfig` from the project folder. It installs everything above. The committed `.vsconfig` names the Visual Studio 2022 compiler (14.44). With Visual Studio 2026, pick the 14.50 compiler and ATL from the table instead.
- **`.vsconfig` changes by itself:** Unreal rewrites it to match the compiler it finds (for example 14.44 → 14.50). Don't commit that change unless every computer uses the same Visual Studio.
- To get a `.sln`: right-click `ZSurvivalGame.uproject` → **Generate Visual Studio project files** (on Windows 11, under **Show more options**). Open `ZSurvivalGame.sln`, choose **Development Editor | Win64** and press F5.

## Getting the project

Assets, maps, models, textures and sounds are stored with Git LFS (the list is in `.gitattributes`). Without it, they arrive as small text files and the editor can't open them.

1. Open a terminal and run `git lfs install` once per computer.
2. Clone the repo as usual. The large files download with it.
3. If you cloned before step 1 (assets fail to load, or `.uasset` files are only a few hundred bytes), run `git lfs pull` in the project folder.

New files of those types go into Git LFS by themselves. To add another kind of file, add a line to `.gitattributes` the same way.

## Before first launch

Double-click `ZSurvivalGame.uproject` and click **Yes** when asked to rebuild the `ZSurvivalGame` module. The first launch also compiles shaders and can take a while.

If it says **"This project was made with a different version of the Unreal Engine"** even though you have 5.8, the launcher has lost track of your engine install. Don't choose **Open a copy**: it makes a second copy of the whole project, with its own `.git`, next to this folder. Instead, cancel, open **Unreal Engine → Library** in the launcher, click the arrow on the 5.8 tile and choose **Verify**, then try again. If you choose **Convert in-place**, nothing gets converted, but `ZSurvivalGame.uproject` gets an engine ID that only that computer has instead of `"5.8"`. Don't commit that line.

The first time you open a city map (`Content/Maps/Zone1_CityDistrict`), the editor also builds the houses' lighting data (distance fields) for 866 meshes. It can look stuck for several minutes: let it finish. After that it's saved on that computer and maps open quickly.

## Things the repo doesn't hold

- **Your saved characters** are in `Saved/SaveGames` (`Slot1.sav`, `Slot2.sav` and their `.bak` files). To keep playing the same character on another computer, copy that folder into the same place there.
