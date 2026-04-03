# Plan: kGit — A TortoiseGit-style Git Manager for Linux

## Context

The goal is to build a native Linux Git GUI that integrates into the GNOME desktop (Nautilus file manager) via right-click context menus, similar to how TortoiseGit works on Windows. The app should provide comprehensive Git version control operations — cloning, committing, branching, diffing, log viewing, etc. — directly from the file manager.

**Key decisions (user confirmed):**
- Platform: Linux only
- Shell integration: Nautilus (GNOME) — right-click context menu
- UI framework: GTKmm (GTK/wayland native)
- Git backend: libgit2

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│  Shell Integration Layer                        │
│  ~/.local/share/file-manager/actions/*.desktop  │
│  (registers right-click menu entries)           │
└──────────────────────┬──────────────────────────┘
                       │ launches app with dir path
                       ▼
┌─────────────────────────────────────────────────┐
│  App Entry Point (main.cpp)                     │
│  - Parse CLI args (directory path)              │
│  - Detect git repo state                        │
│  - Route to appropriate view                    │
└──────────────────────┬──────────────────────────┘
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
┌─────────────────────┐  ┌──────────────────────┐
│  libgit2 Wrapper    │  │  GTKmm UI Layer       │
│  (GitEngine)        │  │  (MainWindow,        │
│                     │  │   dialogs, views)    │
└─────────────────────┘  └──────────────────────┘
```

The app is a **standalone GTKmm application** launched from Nautilus context menus. It is NOT a Nautilus extension written in C — the context menu integration is done via `.desktop` action files (standard Freedesktop mechanism, no Python needed).

---

## Project Structure

```
kGit/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── git_engine.cpp / .hpp       # libgit2 wrapper
│   ├── app.cpp / .hpp              # Application-level logic
│   ├── ui/
│   │   ├── main_window.cpp / .hpp  # Main window
│   │   ├── clone_dialog.cpp / .hpp
│   │   ├── commit_dialog.cpp / .hpp
│   │   ├── diff_view.cpp / .hpp
│   │   ├── log_view.cpp / .hpp
│   │   ├── branch_view.cpp / .hpp
│   │   ├── status_view.cpp / .hpp
│   │   ├── settings_dialog.cpp / .hpp
│   │   └── common.hpp              # Shared widgets, helpers
│   └── utils/
│       ├── diff_tool.cpp / .hpp    # Launching user diff tool
│       └── path_utils.cpp / .hpp
├── data/
│   ├── actions/
│   │   ├── kgit-clone.desktop
│   │   ├── kgit-open.desktop
│   │   └── kgit-init.desktop
│   └── icons/
│       └── kgit.svg
└── tests/
    └── git_engine_test.cpp
```

---

## Phase 1: Project Scaffold & Build System

### 1.1 `CMakeLists.txt`
- Require C++20
- Link: `gtkmm-3.0` (or `gtkmm-4.0`), `libgit2`, `pthread`
- Install desktop files to `~/.local/share/file-manager/actions/`
- Install icons

### 1.2 Desktop Action Files (Shell Integration)
These go in `~/.local/share/file-manager/actions/` (user-local) or `/usr/share/file-manager/actions/` (system-wide).

Files registered (triggered on right-clicking a directory):
- **`kgit-clone.desktop`** — "Clone Git Repository Here" — launches app with `--clone <dir>`
- **`kgit-open.desktop`** — "kGit" — launches app with `--open <dir>` (shows main window)
- **`kgit-init.desktop`** — "Initialize Git Repository" — launches app with `--init <dir>`

Each `.desktop` file uses `MimeType=inode/directory;` and `Exec=kgit --open %f` (or similar).

### 1.3 App Entry Point (`main.cpp`)
```cpp
// Routes:
//   --clone <path>    → CloneDialog
//   --init <path>     → init repo → CommitDialog
//   --open <path>     → MainWindow with that repo
//   (no args)         → repo picker / empty window
```

---

## Phase 2: libgit2 GitEngine Layer

A thin, clean wrapper around libgit2 exposing the operations needed.

### Core operations to implement:

| Operation | libgit2 calls |
|---|---|
| Clone | `git_clone` |
| Init repo | `git_repository_init` |
| Add file | `git_index_add_bypath` |
| Remove file | `git_index_remove` |
| Move/rename | `git_index_entryrename` |
| Stage/unstage | `git_index_add_*` / `git_index_remove` |
| Commit | `git_commit_create` / `git_signature` |
| Revert file | `git checkout from HEAD for path` |
| Revert all | `git_checkout_head` |
| Diff (working vs index) | `git_diff_index_to_workdir` |
| Diff (index vs HEAD) | `git_diff_head_to_index` |
| Diff (commit vs commit) | `git_diff_tree_to_tree` |
| Log / commit history | `git_log` |
| File history | `git_log on specific path` |
| Status (all states) | `git_status_list` |
| Show tree at commit | `git_commit_tree` → `git_tree_entry*` |
| Branch create | `git_branch_create` |
| Branch list | `git_branch_iterator` |
| Branch delete | `git_branch_delete` |
| Checkout (branch/tag/commit) | `git_checkout_tree` |
| Remote add | `git_remote_create` |
| Remote list/delete | `git_remote_*` |
| Fetch (no merge) | `git_fetch` |
| Push | `git_push` |
| Pull (no auto-merge) | fetch + manual rebase/merge user choice |
| Merge | `git_merge` |
| Tags | `git_tag_*` |

### Auth handling for remotes:
- SSH key auth: libgit2 supports `GIT_SSH_KEY` and `GIT_SSH_COMMAND` environment
- HTTPS + credential prompt: GTK dialog asking for username/password, cache in memory (no persistent storage unless keyring integration is added)
- Credential helper: delegate to `git credential fill` subprocess as fallback

---

## Phase 3: GTKmm UI — Main Window

**Main window** with:
- **Toolbar** at top: Clone, Commit, Push, Pull, Fetch, Merge, Create Branch, Checkout
- **Left panel** (GTreeView): file browser within the repo — shows working copy tree
  - Icons per file: modified, added, deleted, unversioned, ignored
  - Right-click on file: diff, revert, delete, rename, view history
- **Right panel** (GNotebook):
  - **Changes tab**: status list (staged/unstaged)
  - **Log tab**: commit history graph (like gitg), commit details pane
  - **Diff tab**: side-by-side diff of selected file
  - **Branches tab**: local/remote branches, tags
  - **Repo browser tab**: view repo contents at any commit

**Window behavior:**
- Opens to the directory passed via `--open <path>`
- Detects if path is inside a git repo, walks up to find `.git`
- If not a git repo: shows "Not a git repository" with Init / Clone options

---

## Phase 4: Dialogs & Views

### Clone Dialog
- URL entry (with validation)
- Local directory (default: same dir / subdir)
- Branch (optional, default: all)
- SSH key selector (list `~/.ssh/` keys)
- Credential entry if HTTPS

### Commit Dialog
- Staged files list (top) + unstaged files list (bottom)
- Commit message (GtkTextView, monospace)
- Amend checkbox (edits last commit message)
- Sign-off / GPG sign checkboxes

### Diff View
- File list with change type icons
- Side-by-side text diff (custom GtkTextView-based or use `git diff` output + highlight)
- External diff tool launcher button (calls user-configured `git diff.tool`, falls back to configured tool path)
- Per-file revert button

### Log View
- Commit graph (columns for branching/merging visualization)
- Columns: graph chars, author, date, message, hash (short)
- Click to see full diff, or view full commit info
- Filter by: author, date range, file, message regex

### Branch/Tag Manager
- List local and remote branches
- Create branch (from any ref, with tracking)
- Delete branch (with force option)
- Checkout branch/tag/commit
- Create/delete tags (annotated vs lightweight)

### Repo Browser
- Tree view of repo at any commit
- Navigate history by commit selector
- Click file to view raw content or blame

### Settings Dialog
- Default diff tool path + arguments
- SSH key default path
- Credential helper preference
- Theme (follow system / light / dark via CSS)

---

## Phase 5: Shell Integration (Nautilus)

### Installation
Desktop action files are copied to `~/.local/share/file-manager/actions/` on install.

### How it works
Freedesktop.org's file-manager actions spec lets you register menu entries for MIME types. For `inode/directory`, this appears on any folder right-click.

Example `kgit-open.desktop`:
```ini
[Desktop Entry]
Name=kGit
Comment=Open Git repository
MimeType=inode/directory;
Exec=kgit --open %f
Icon=kgit
Type=Action
```

Upon install: `update-desktop-database ~/.local/share/applications` is run.

### Self-contained no-install option
Since desktop action files go to user-local dirs, a `make install` or install script copies them. No root needed.

---

## Phase 6: Build, Test & Install

### Dependencies
```bash
# Debian/Ubuntu
sudo apt install libgtkmm-3.0-dev libgit2-dev cmake meson ninja-build
```

### Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install  # copies binaries + desktop files
```

### Verification
- Right-click any folder in Nautilus → "GitNautilus" appears
- Click → app opens showing repo status
- Clone a repo, commit, push, branch operations all work

---

## Open Questions / Things to Watch

1. **libgit2 version**: Different distros ship different libgit2 versions. Recommend **bundling libgit2 as a subdirectory** (`src/libgit2/`) compiled statically, to avoid distro fragmentation and ensure latest features. This is what many Linux apps do (e.g. `gitg`, `git-delta`).
2. **SSH auth complexity**: Full SSH key agent forwarding is tricky. Start with `GIT_SSH_COMMAND` approach, support `~/.ssh/id_rsa` / `id_ed25519` default keys, and prompt for key path if auth fails.
3. **Large repos**: libgit2 is fast but the GTK UI must handle repos with 100k+ files gracefully. Use lazy loading for status, incremental loading.
4. **Diff tool**: Use `git difftool` invocation as default (delegates to user's git config), with ability to specify custom tool.
5. **Merge conflicts**: Show a 3-way merge view when merge conflicts occur. GTK has no built-in merge UI — may need to use `gtkimerge` or build a simple 3-pane diff.
6. **Nautilus version compatibility**: Desktop action files are standard but verify they work on Nautilus 42+ (Ubuntu 22.04+) and 45+ (Ubuntu 24.04+).
7. **Wayland vs X11**: GTKmm on wayland should work fine with `GDK_BACKEND=wayland`. The app doesn't need X-specific APIs.

---

## Implementation Order

1. **Scaffold** — CMake, basic main.cpp, CMake dep resolution
2. **GitEngine** — libgit2 wrapper, basic operations (status, clone, commit)
3. **Basic UI** — GTKmm window, status panel, commit dialog
4. **Shell integration** — desktop action files, verify right-click works
5. **Full operations** — diff, log, branches, remotes, push/pull, tags
6. **Polish** — settings, theming, error handling, large repo optimization
