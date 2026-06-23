# Mycel

Mycel is a C++/Qt mind-map tool that uses the system's folder and file structure as its data source.

Instead of treating files as a vertical file list, Mycel lays out folders and files as connected nodes on a whiteboard-style canvas. It is designed for exploring project structure, understanding document sets, and organizing information as a map while keeping the underlying data in normal system folders and files.

Japanese documentation is available in [README.ja.md](README.ja.md).

Current version: 0.2.7.

Release history is available in [CHANGELOG.md](CHANGELOG.md).

## Features

- Full-screen whiteboard-style canvas
- Large virtual canvas area for free panning around the map
- Directory structure rendered as connected mind-map nodes
- Folder and file creation from the canvas
- Drag-and-drop moving between folders
- File-to-file linking by dropping a file on the right edge of another file
- Drag-and-drop reordering across files and folders
- Persistent item order in `.mycel/order.json`
- User-assigned node colors in `.mycel/colors.json`
- Preview open/closed state and custom preview sizes in `.mycel/previews.json`
- File-to-file links in `.mycel/links.json`
- Canvas view and window state in `.mycel/view.json`
- Folder collapse and expand
- Inline previews for text and Markdown files
- Selectable inline preview text with copy support
- Direct editor pane for the selected editable text file, with automatic saving
- Toggleable editor pane
- Editor pane placement can be switched between left, right, and bottom
- Built-in editor for text files, with Ctrl + S save and Ctrl + mouse wheel zoom
- Persistent editor font size shared across editable files and sessions
- Persistent canvas zoom, view position, window size, and maximized/full-screen state
- Markdown preview rendering with wrapped text
- Multi-selection with batch preview open/close and selected item deletion
- Context menus for refresh, rename, delete, color, open, and creation actions
- Selection-based file and folder creation with `N` and `Shift + N`
- Copy selected files and folders with `Ctrl + C` / `Ctrl + V`
- Keyboard navigation across visible nodes with `Tab` and `Shift + Tab`
- Focus the editor pane with `E`
- Toggle selected file previews with `V`
- Trackpad pinch zoom and mouse wheel zoom
- Range zoom: drag an empty canvas area, then press Enter to zoom to that range
- Fit-to-map shortcut with Ctrl + 0
- Keyboard shortcuts for reload, maximize, inline rename, and the cheat sheet
- Startup root selection: current directory by default, or the first command-line argument when provided
- `--version` option and title-bar version display

## What's New in 0.2.7

- Added file-to-file links: drop a file on the right edge of another file to create a relationship line without moving the file, persisted in `.mycel/links.json`.
- Placed linked target files to the right of their source file and adjusted spacing to reduce preview overlap.
- Changed linked file connections from dashed lines to normal solid lines, and hid the target file's original folder connection while linked.
- Added a file context-menu action to remove an incoming file link and restore the original folder connection.

## What's New in 0.2.6

- Added an editor placement menu for left, right, and bottom layouts, with the selected placement persisted across sessions.
- Saved canvas zoom, view position, window size, and maximized/full-screen state in `.mycel/view.json`, then restored them on the next launch.
- Fixed startup synchronization so the editor pane is shown when the Editor toggle is on.
- Added Ctrl + S inside the editor pane to save and return focus to the edited file.
- Added Subject-based renaming: `Subject: filename.txt` on the first line renames the file after saving and removes that `Subject:` line from the file.
- Hid lines starting with `Subject:` or `Key:` from inline previews.

## What's New in 0.2.5

- Collapsed large root trees on startup so only the root and its direct folders are shown initially.
- Added `N` / `Shift + N` shortcuts to create `NewFile.txt` / `NewFolder` from the selected folder.
- Changed `N` on a selected file to create `NewFile.txt` in that file's parent folder.
- Added `Ctrl + C` / `Ctrl + V` copy support for selected files and folders in their parent folders.
- Added `E` to focus the selected editable file in the right-side editor pane.
- Added `V` to toggle selected file previews; with multiple files, all open previews close, otherwise all selected file previews open.
- Added `Tab` / `Shift + Tab` navigation through visible same-level items.
- Preserved canvas focus and selection after `N`, `Shift + N`, `V`, and `Tab` operations.
- Refreshed an open inline preview after automatic saves from the right-side editor pane.
- Improved multi-selection dragging, folder drops, and deletion for mixed file/folder selections.
- Highlighted folder drop targets while dragging items.

## What's New in 0.2.4

- Added single-file preview open/close actions to the top of the file context menu.
- Moved preview open/close actions to the top of the multi-selection context menu.
- Replaced the color dialog with a fixed color palette in the context menu.
- Changed node color display from colored connector lines to soft backgrounds behind the file name, icon, and preview.
- Added text selection and copy support inside inline previews.
- Added a Copy context menu to inline preview text.
- Changed canvas zoom so the mouse wheel zooms without holding Ctrl.
- Added `--version` output and version display in the window title.
- Added a right-side editor pane that opens the selected editable text file and saves edits automatically.
- Persisted editor font size changes from Ctrl + mouse wheel across files and sessions.
- Added an Editor toolbar toggle and Ctrl + E shortcut to show or hide the right-side editor pane.

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

Show the version and exit:

```sh
./build/mycel --version
```

On normal startup, if the root directory does not contain a `.mycel` folder, Mycel asks whether to create one, open the directory in `--no-mycel` mode, or choose another root directory.

In `--no-mycel` mode, Mycel does not load or create `.mycel/order.json`, `.mycel/colors.json`, `.mycel/previews.json`, `.mycel/links.json`, or `.mycel/view.json`. Reordering, node color changes, file-to-file linking, and persisted view/window restoration are disabled in this mode.

## Mouse Operations

- Empty canvas drag: select a range of nodes
- ?: show the cheat sheet
- Enter after empty canvas drag: zoom to the dragged range
- Ctrl + 0: fit the whole map
- Ctrl + E: show or hide the editor pane
- Toolbar Editor Place: move the editor pane to the left, right, or bottom
- E: edit the selected file in the editor pane
- Ctrl + S inside the editor pane: save and return focus to the edited file; if the first line is `Subject: filename.txt`, rename the file to `filename.txt` after saving and remove that `Subject:` line from the file
- Lines starting with `Subject:` or `Key:` are hidden from inline previews
- Tab: move to the next visible item at the same level
- Shift + Tab: move to the previous visible item at the same level
- N: create `NewFile.txt` in the selected folder or in the selected file's folder
- Shift + N: create `NewFolder` in the selected folder
- V: toggle selected file previews
- Drop a file on the right edge of another file: link the files
- Right-click a linked target file and choose `関連を解除`: restore its original folder connection
- Ctrl + C / Ctrl + V: copy selected items into their parent folders
- F5: reload the whole map
- F11: maximize or restore the window
- Alt + left drag, middle-button drag, or right-button drag on empty canvas space: pan the canvas
- Trackpad pinch: zoom
- Mouse wheel: zoom
- Trackpad scroll: scroll the canvas
- Ctrl + left-click a node: add or remove it from the current selection
- Shift + file click: open or close that file's preview
- Shift + folder click: collapse or expand that folder
- Double-click a folder: create a new file in that folder
- Shift + double-click a folder: create a new folder named `NewFolder` inside that folder
- Double-click a file: open it with the OS default application
- F2 with a single file or folder selected: rename it inline
- Select an editable text file: edit it directly in the right-side editor pane
- Ctrl + mouse wheel in an editor: change editor font size and keep it for other files and future sessions
- Drop files or folders from the OS onto a folder node: copy them into that folder
- Drag a file or folder node: reorder it inside the same folder
- Drag a file or folder node onto a folder: move it into that folder
- Drag the lower-right corner of a preview: resize the preview
- Select text in a preview, then press Ctrl + C or right-click and choose Copy
- Right-click a node: open its context menu

## Context Menus

Single file menu:

- Open preview
- Close preview
- Refresh this item
- Refresh all
- Edit text files
- Rename
- Delete
- Choose from fixed node colors or clear color
- Open

Multiple selected files menu:

- Open previews
- Close previews
- Refresh selection
- Refresh all
- Delete selected files

Folder menu:

- Refresh this item
- Refresh all
- Collapse or expand
- Create folder
- Create file
- Paste clipboard contents
- Delete folder
- Choose from fixed node colors or clear color
- Open

## Samples

The `Sample` directory contains a business process reform mind-map sample. Open it with:

```sh
./build/mycel Sample
```

It includes nested folders and Markdown notes for current-state analysis, issue structure, reform themes, measures, KPIs, roadmap, communication, risk management, and meeting notes.

## Notes

Mycel writes its own metadata into a `.mycel` directory under the opened root folder. This stores local layout-related information such as custom ordering, colors, and preview state.
