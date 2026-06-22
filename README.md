# Mycel

Mycel is a C++/Qt mind-map tool that uses the system's folder and file structure as its data source.

Instead of treating files as a vertical file list, Mycel lays out folders and files as connected nodes on a whiteboard-style canvas. It is designed for exploring project structure, understanding document sets, and organizing information as a map while keeping the underlying data in normal system folders and files.

Japanese documentation is available in [README.ja.md](README.ja.md).

Current version: 0.2.3.

Release history is available in [CHANGELOG.md](CHANGELOG.md).

## Features

- Full-screen whiteboard-style canvas
- Large virtual canvas area for free panning around the map
- Directory structure rendered as connected mind-map nodes
- Folder and file creation from the canvas
- Drag-and-drop moving between folders
- Drag-and-drop reordering across files and folders
- Persistent item order in `.mycel/order.json`
- User-assigned node colors in `.mycel/colors.json`
- Preview open/closed state and custom preview sizes in `.mycel/previews.json`
- Folder collapse and expand
- Inline previews for text and Markdown files
- Built-in editor for text files, with Ctrl + S save and Ctrl + mouse wheel zoom
- Markdown preview rendering with wrapped text
- Multi-selection with batch preview open/close and selected item deletion
- Context menus for refresh, rename, delete, color, open, and creation actions
- Trackpad pinch zoom and Ctrl + mouse wheel zoom
- Range zoom: drag an empty canvas area, then press Enter to zoom to that range
- Fit-to-map shortcut with Ctrl + 0
- Keyboard shortcuts for reload, maximize, inline rename, and the cheat sheet
- Startup root selection: current directory by default, or the first command-line argument when provided

## What's New in 0.2.3

- Added a built-in editor for text files from the file context menu.
- Added Ctrl + S save, unsaved-change prompts, and external-change overwrite confirmation in the editor.
- Added Ctrl + mouse wheel zoom inside the built-in text editor.
- Expanded the virtual whiteboard canvas so the map can be freely panned far beyond its nodes.

## What's New in 0.2.2

- Updated selection visuals so selected files and folders use the same clear framed highlight around the icon and name.
- Removed normal folder and root node frames so the canvas only emphasizes selected items and active drop targets.
- Persisted inline preview state in `.mycel/previews.json`, including open/closed state and custom preview width/height.
- Restored saved preview state on startup, directory open, and full reload.
- Added right-button drag on empty canvas space as another way to pan the canvas.

## What's New in 0.2.1

- Added Ctrl + left-click multi-selection.
- Added Shift + folder double-click to create `NewFolder` without a dialog.
- Added F2 inline rename for a single selected file or folder.
- Added F5 reload, F11 maximize/restore, and `?` cheat sheet shortcuts.
- Fixed startup root handling so the app opens the current directory by default and the argument directory when provided.

## Build

Use the platform build scripts:

macOS:

```sh
./scripts/build-mac.sh
```

Linux:

```sh
./scripts/build-linux.sh
```

Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

On Windows, the script can auto-detect Qt installed under `C:\Qt`. It prefers an MSVC Qt kit such as `C:\Qt\6.x\msvc2022_64` when available, imports the Visual Studio C++ environment, and builds a GUI-subsystem executable that does not open a terminal window at startup. With an MSVC kit, the default build directory is `build-windows-msvc`.

You can also build manually with CMake:

```sh
cmake -S . -B build
cmake --build build
```

Qt 6 Widgets is required.

If Qt was installed with the Qt online installer and `cmake` is not in `PATH`, use Qt's bundled CMake:

```sh
/Users/fuk/Qt/Tools/CMake/CMake.app/Contents/bin/cmake -S . -B build -DCMAKE_PREFIX_PATH=/Users/fuk/Qt/6.11.1/macos
/Users/fuk/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build
```

### Build Script Options

The macOS and Linux scripts read these environment variables:

- `BUILD_DIR`: build directory. Default: `build`
- `BUILD_TYPE`: CMake build type. Default: `Release`
- `CMAKE_BIN`: CMake executable. Default: `cmake`
- `CMAKE_PREFIX_PATH`: Qt installation path when Qt is not discoverable automatically

Example:

```sh
CMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64 ./scripts/build-linux.sh
```

The Windows script accepts parameters:

```powershell
.\scripts\build-windows.ps1 -BuildType Release -CMakePrefixPath "C:\Qt\6.x\msvc2022_64"
```

If you use MinGW or a specific generator, pass `-Generator`:

```powershell
.\scripts\build-windows.ps1 -Generator "Ninja" -CMakePrefixPath "C:\Qt\6.x\mingw_64"
```

### Windows Installer

After building and deploying the Windows output, create an Inno Setup installer:

```powershell
.\scripts\package-windows-inno.ps1
```

The installer script uses the existing files in `build-windows-msvc` and does not rebuild Mycel. It requires Inno Setup's `ISCC.exe`; pass `-IsccPath` if it is not installed in a standard location.

## GitHub Actions

The repository includes a GitHub Actions workflow at `.github/workflows/build.yml`.

It builds Mycel on:

- macOS
- Linux
- Windows

The workflow installs Qt 6, configures the native compiler environment, and runs the platform build scripts.

## Run

Open the current directory:

```sh
./build/mycel
```

Windows MSVC build:

```powershell
.\build-windows-msvc\mycel.exe
```

Open another directory:

```sh
./build/mycel /path/to/project
```

Run without reading or creating `.mycel` metadata:

```sh
./build/mycel --no-mycel /path/to/project
```

On normal startup, if the root directory does not contain a `.mycel` folder, Mycel asks whether to create one, open the directory in `--no-mycel` mode, or choose another root directory.

In `--no-mycel` mode, Mycel does not load or create `.mycel/order.json`, `.mycel/colors.json`, or `.mycel/previews.json`. Reordering and node color changes are disabled in this mode.

## Mouse Operations

- Empty canvas drag: select a range of nodes
- ?: show the cheat sheet
- Enter after empty canvas drag: zoom to the dragged range
- Ctrl + 0: fit the whole map
- F5: reload the whole map
- F11: maximize or restore the window
- Alt + left drag, middle-button drag, or right-button drag on empty canvas space: pan the canvas
- Trackpad pinch: zoom
- Ctrl + mouse wheel: zoom
- Mouse wheel or trackpad scroll: scroll the canvas
- Ctrl + left-click a node: add or remove it from the current selection
- Shift + file click: open or close that file's preview
- Shift + folder click: collapse or expand that folder
- Double-click a folder: create a new file in that folder
- Shift + double-click a folder: create a new folder named `NewFolder` inside that folder
- Double-click a file: open it with the OS default application
- F2 with a single file or folder selected: rename it inline
- Drop files or folders from the OS onto a folder node: copy them into that folder
- Drag a file or folder node: reorder it inside the same folder
- Drag a file or folder node onto a folder: move it into that folder
- Drag the lower-right corner of a preview: resize the preview
- Right-click a node: open its context menu

## Context Menus

Single file menu:

- Refresh this item
- Refresh all
- Edit text files
- Rename
- Delete
- Set or clear color
- Open

Multiple selected files menu:

- Refresh selection
- Refresh all
- Open previews
- Close previews
- Delete selected files

Folder menu:

- Refresh this item
- Refresh all
- Collapse or expand
- Create folder
- Create file
- Paste clipboard contents
- Delete folder
- Set or clear color
- Open

## Samples

The `Sample` directory contains a business process reform mind-map sample. Open it with:

```sh
./build/mycel Sample
```

It includes nested folders and Markdown notes for current-state analysis, issue structure, reform themes, measures, KPIs, roadmap, communication, risk management, and meeting notes.

## Notes

Mycel writes its own metadata into a `.mycel` directory under the opened root folder. This stores local layout-related information such as custom ordering, colors, and preview state.
