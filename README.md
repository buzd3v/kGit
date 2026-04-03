# kGit — TortoiseGit-style Git Manager for Linux

A native Linux Git GUI that integrates into the GNOME desktop (Nautilus) via right-click context menus, built with GTKmm and libgit2.

## Features

- **TortoiseGit-style UX** — right-click any folder in Nautilus to open, clone, or init a Git repository
- **Full Git operations** — clone, commit, push, pull, fetch, merge, branch, tag, checkout, diff, log
- **Native GTKmm UI** — fast, responsive, follows system theme (light/dark)
- **No root required** — desktop action files install to user-local directory

## Build

```bash
# Dependencies (Debian/Ubuntu)
sudo apt install libgtkmm-3.0-dev libgit2-dev cmake ninja-build

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
make install    # installs binary + desktop actions + icon
```

## Usage

Right-click any folder in Nautilus to see kGit options:
- **kGit — Open Repository** — opens the repository in kGit
- **Clone Git Repository Here** — clone a repo into the selected directory
- **Initialize Git Repository** — create a new repo

Or from the command line:
```bash
kgit --open <path>      # open a repository
kgit --clone <path>    # clone into directory
kgit --init <path>     # initialize new repo
```

## Architecture

```
src/
├── main.cpp              Entry point
├── app.cpp/.hpp          CLI routing + application lifecycle
├── git_engine.cpp/.hpp   libgit2 wrapper — all Git operations
├── ui/
│   ├── main_window.cpp    Main window (toolbar, file tree, panels)
│   ├── clone_dialog.cpp   Clone repository dialog
│   ├── commit_dialog.cpp  Commit dialog (staged/unstaged + message)
│   ├── diff_view.cpp     Side-by-side diff view
│   ├── log_view.cpp      Commit history browser
│   ├── branch_view.cpp    Branch/tag management
│   ├── status_view.cpp    Working tree status
│   └── settings_dialog.cpp Preferences
└── utils/
    ├── diff_tool.cpp      External diff tool launcher
    └── path_utils.cpp     Path normalization, git root detection

data/
├── actions/              Nautilus desktop action files
└── icons/kgit.svg        Application icon
```

## Tech Stack

- **C++20** — modern C++ throughout
- **GTKmm 3.0** — native GTK UI framework
- **libgit2** — Git operations (no subprocess calls except credential helper)
- **CMake** — build system

## Shell Integration

kGit registers with Nautilus via Freedesktop `.desktop` action files installed to `~/.local/share/file-manager/actions/`. No Python, no root, no Nautilus extension required.
