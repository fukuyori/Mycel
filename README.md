# Mycel

Mycel is a C++/Qt mind-map tool that uses the system's folder and file structure as its data source.

Instead of treating files as a vertical file list, Mycel lays out folders and files as connected nodes on a whiteboard-style canvas. It is designed for exploring project structure, understanding document sets, and organizing information as a map while keeping the underlying data in normal system folders and files.

- Current version: 0.2.8
- Release history: [CHANGELOG.md](CHANGELOG.md)
- Japanese documentation: [README.ja.md](README.ja.md)

## Features

### Mind-Map View

- Folders and files rendered as connected nodes
- Large whiteboard-style canvas with free pan and zoom
- Folder collapse and expand
- Mixed file/folder ordering
- File-to-file linking by dropping a file on the right edge of another file
- Linked target files placed to the right of their source file
- Multi-selection, range selection, and range zoom

### File Operations

- Create `NewFile.txt` / `NewFolder` from the canvas
- Move items between folders by drag and drop
- Copy OS files and folders by dropping them onto folder nodes
- Copy selected items with `Ctrl + C` / `Ctrl + V`
- Inline rename with F2
- Context menus for refresh, delete, color, creation, and open actions

### Preview And Editing

- Inline previews for text and Markdown files
- Select and copy text inside previews
- Lightweight preview frames for images, PDFs, and other files
- Editor pane for directly editing the selected editable text file
- Editor pane placement can be switched between left, right, and bottom
- Automatic saving, plus `Ctrl + S` to save and return focus to the file
- Persistent editor font size changed with `Ctrl + mouse wheel`
- Rename by putting `Subject: filename.txt` on the first line
- Lines starting with `Subject:` or `Key:` are hidden from inline previews

### Persisted State

Mycel writes local metadata into a `.mycel` directory under the opened root folder.

| File | Contents |
| --- | --- |
| `.mycel/order.json` | Custom ordering |
| `.mycel/colors.json` | User-assigned node colors |
| `.mycel/previews.json` | Preview open/closed state and size |
| `.mycel/links.json` | File-to-file links |
| `.mycel/view.json` | Canvas view, window size, maximized state, and full-screen state |

In `--no-mycel` mode, Mycel does not load or create these files. Reordering, node color changes, file-to-file linking, and persisted view/window restoration are disabled in this mode.

Recent root-folder history is saved in the application settings. Mycel stores normalized absolute paths, removes duplicates, and keeps up to 20 entries.

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

Show the version and exit:

```sh
./build/mycel --version
```

On normal startup, if the root directory does not contain a `.mycel` folder, Mycel asks whether to create one, open the directory in `--no-mycel` mode, choose a root folder from history, or choose another root directory.

## Controls

### Basic View Controls

| Operation | Action |
| --- | --- |
| Mouse wheel | Zoom |
| Trackpad pinch | Zoom |
| Trackpad scroll | Scroll the canvas |
| Alt + left drag | Pan the canvas |
| Middle-button drag | Pan the canvas |
| Right-button drag on empty canvas space | Pan the canvas |
| Left drag on empty canvas space | Select a range |
| Enter after range selection | Zoom to the selected range |
| Ctrl + 0 | Fit the whole map |
| F5 | Reload the whole map |
| F11 | Maximize or restore the window |
| ? | Show the cheat sheet |

### Selection And Navigation

| Operation | Action |
| --- | --- |
| Left-click a node | Select it |
| Ctrl + left-click a node | Add or remove it from the current selection |
| Tab | Move to the next visible item at the same level |
| Shift + Tab | Move to the previous visible item at the same level |
| F2 with one file or folder selected | Rename it inline |

### File And Folder Operations

| Operation | Action |
| --- | --- |
| Double-click a folder | Create `NewFile.txt` in that folder |
| Shift + double-click a folder | Create `NewFolder` in that folder |
| N | Create `NewFile.txt` in the selected folder or selected file's folder |
| Shift + N | Create `NewFolder` in the selected folder |
| Double-click a file | Open it with the OS default application |
| Drag a file or folder node | Reorder it inside the same folder |
| Drag a file or folder node onto a folder | Move it into that folder |
| Drop OS files or folders onto a folder node | Copy them into that folder |
| Ctrl + C / Ctrl + V | Copy selected items into their parent folders |

### Preview And Editor

| Operation | Action |
| --- | --- |
| Shift + file click | Open or close that file's preview |
| V | Toggle selected file previews |
| Drag the lower-right corner of a preview | Resize the preview |
| Select preview text and press Ctrl + C | Copy text |
| E | Edit the selected file in the editor pane |
| Ctrl + E | Show or hide the editor pane |
| Toolbar Editor Place | Move the editor pane to the left, right, or bottom |
| Ctrl + S inside the editor pane | Save and return focus to the edited file |
| Ctrl + mouse wheel in an editor | Change editor font size |

### File Links

| Operation | Action |
| --- | --- |
| Drop a file on the right edge of another file | Link the files |
| Right-click a linked target file and choose `関連を解除` | Restore its original folder connection |

## Context Menus

### Single File

- Open/close preview
- Remove link
- Refresh this item
- Refresh all
- Edit
- Rename
- Delete
- Choose from fixed node colors or clear color
- Open

### Multiple Selection

- Open/close selected file previews
- Refresh selection
- Refresh all
- Delete selected items

### Folder

- Refresh this item
- Refresh all
- Collapse or expand
- Create folder
- Create file
- Paste clipboard contents
- Delete
- Choose from fixed node colors or clear color
- Open

## Build

Qt 6 Widgets is required.

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

### Build Script Options

The macOS and Linux scripts read these environment variables:

| Variable | Description |
| --- | --- |
| `BUILD_DIR` | Build directory. Default: `build` |
| `BUILD_TYPE` | CMake build type. Default: `Release` |
| `CMAKE_BIN` | CMake executable. Default: `cmake` |
| `CMAKE_PREFIX_PATH` | Qt installation path when Qt is not discoverable automatically |

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

If you select the `Add Mycel to the user PATH` additional task, the installer adds the install directory to the user PATH so newly opened terminals can run `mycel`. The entry is removed during uninstall.

## GitHub Actions

The repository includes a GitHub Actions workflow at `.github/workflows/build.yml`. It installs Qt 6 and runs the platform build scripts on macOS, Linux, and Windows.

## Samples

The `Sample` directory contains a business process reform mind-map sample. Open it with:

```sh
./build/mycel Sample
```

It includes nested folders and Markdown notes for current-state analysis, issue structure, reform themes, measures, KPIs, roadmap, communication, risk management, and meeting notes.
