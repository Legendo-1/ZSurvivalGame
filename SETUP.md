# Setup

Things you need to install or do yourself after cloning. Everything else comes with the repo.

## Install

- **Unreal Engine 5.8** (made with 5.8.3), from the Epic Games Launcher
- **Visual Studio 2022**: in the Visual Studio Installer, click **More → Import configuration** and choose `.vsconfig` from the project folder. This installs the required C++ workloads, the MSVC 14.50 toolset and Windows SDK 10.0.22621.
- **Git**

## Before first launch

1. Right-click `ZSurvivalGame.uproject` → **Switch Unreal Engine version** → choose **5.8**.
2. Double-click `ZSurvivalGame.uproject` and click **Yes** when asked to rebuild the `ZSurvivalGame` module. The first launch also compiles shaders and can take a while.
