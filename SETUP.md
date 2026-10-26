# Setup

**Last updated:** 2026-10-26 (Git LFS)

Things you need to install or do yourself after cloning. Everything else comes with the repo.

## Install

- **Unreal Engine 5.8** (made with 5.8.3), from the Epic Games Launcher
- **Visual Studio 2022**: in the Visual Studio Installer, click **More → Import configuration** and choose `.vsconfig` from the project folder. This installs the required C++ workloads, the MSVC 14.50 toolset and Windows SDK 10.0.22621.
- **Git**, with **Git LFS** (Large File Storage). The Git for Windows installer includes it.

## Getting the project

Assets, maps, models, textures and sounds are stored with Git LFS (the list is in `.gitattributes`). Without it, they arrive as small text files and the editor can't open them.

1. Open a terminal and run `git lfs install` once per computer.
2. Clone the repo as usual. The large files download with it.
3. If you cloned before step 1 (assets fail to load, or `.uasset` files are only a few hundred bytes), run `git lfs pull` in the project folder.

New files of those types go into Git LFS by themselves. To add another kind of file, add a line to `.gitattributes` the same way.

## Before first launch

Double-click `ZSurvivalGame.uproject` and click **Yes** when asked to rebuild the `ZSurvivalGame` module. The first launch also compiles shaders and can take a while.
