# Mycel

Mycel is a C++/Qt file browser that visualizes a directory as a whiteboard-style mind map.

Instead of showing files as a vertical list, Mycel lays out folders and files as connected nodes. It is designed for exploring project structure, understanding document sets, and navigating information as a map.

Japanese documentation is available in [README.ja.md](README.ja.md).

Current version: 0.2.0.

## Features

- Full-screen whiteboard-style canvas
- Directory structure rendered as connected mind-map nodes
- Folder and file creation from the canvas
- Drag-and-drop moving between folders
- Drag-and-drop reordering across files and folders
- Persistent item order in `.mycel/order.json`
- User-assigned node colors in `.mycel/colors.json`
- Folder collapse and expand
- Inline previews for text and Markdown files
- Markdown preview rendering with wrapped text
- Multi-selection with batch preview open/close and batch file deletion
- Context menus for refresh, rename, delete, color, open, and creation actions
- Trackpad pinch zoom and Ctrl + mouse wheel zoom
- Range zoom: drag an empty canvas area, then press Enter to zoom to that range
- Fit-to-map shortcut with Ctrl + 0

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

Qt 6 Widgets is preferred. The CMake file falls back to Qt 5 Widgets if Qt 6 is not available.

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

## Mouse Operations

- Empty canvas drag: select a range of nodes
- Enter after empty canvas drag: zoom to the dragged range
- Ctrl + 0: fit the whole map
- Alt + left drag or middle-button drag: pan the canvas
- Trackpad pinch: zoom
- Ctrl + mouse wheel: zoom
- Mouse wheel or trackpad scroll: scroll the canvas
- Shift + file click: open or close that file's preview
- Shift + folder click: collapse or expand that folder
- Double-click a folder: create a new file in that folder
- Double-click a file: open it with the OS default application
- Drag a file or folder node: reorder it inside the same folder
- Drag a file or folder node onto a folder: move it into that folder
- Drag the lower-right corner of a preview: resize the preview
- Right-click a node: open its context menu

## Context Menus

Single file menu:

- Refresh this item
- Refresh all
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

Mycel writes its own metadata into a `.mycel` directory under the opened root folder. This stores local layout-related information such as custom ordering and colors.
