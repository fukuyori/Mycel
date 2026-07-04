# Mycel

Mycel is a C++/Qt mind-map tool that uses the system's folder and file structure as its data source.

Instead of treating files as a vertical file list, Mycel lays out folders and files as connected nodes on a whiteboard-style canvas. It is designed for exploring project structure, understanding document sets, and organizing information as a map while keeping the underlying data in normal system folders and files.

- Current version: 0.6.8
- Release history: [CHANGELOG.md](CHANGELOG.md)
- Documentation index: [docs/README.ja.md](docs/README.ja.md)
- Development plan: [docs/development-plan.ja.md](docs/development-plan.ja.md)
- Japanese documentation: [README.ja.md](README.ja.md)

## Features

### Mind-Map View

- Folders and files rendered as connected nodes
- Large whiteboard-style canvas with free pan and zoom
- Light and dark themes
- Folder collapse and expand
- Mixed file/folder ordering
- File-to-file linking by dropping a file on the right edge of another file; a folder can also be linked to the right of a file (its subtree expands there)
- Linked target files placed to the right of their source file
- Type-aware file icons with per-type badges and accent colors (PY, GO, MD, CSV, PDF, images, audio, video, and more)
- Multi-selection, range selection, and range zoom
- Hierarchical `.mycel` roots: a child folder that has its own `.mycel` is shown as a Mycel-badged sub-root boundary (its contents are not expanded); double-click it to switch the board into that root. The parent root is drawn to the left of the root node, connected by an edge, and double-clicking it switches back up. Window position and size are kept when switching roots
- Turn a folder into a child root ("子ルートにする") or integrate a sub-root back into its parent ("親に統合"), merging the child's metadata into the parent, from the folder/sub-root context menu

### Board Mode (free-form card layout)

- Toggle between the tree and the board from the View menu / toolbar. All files (folders excluded) appear as cards that can be placed freely — no snapping, unbounded working area
- Layouts are saved as **patterns** (one pattern = one JSON under `.mycel/boards/`, no limit) with create/rename/delete and a per-pattern view; the last mode and pattern are restored on startup
- Initial layout is one row per folder (root-direct files first, then 1st-level folders in tree order, …) with row tops aligned and even spacing based on visual width
- Card context menu: preview, open, open with, color, and hide (from the pattern); real files are never deleted or renamed from the board
- The hidden-cards dialog lists hidden and not-yet-placed cards (files added later) and calls them onto the board; cards whose real file disappeared leave the screen automatically
- Card moves, hide, and show are integrated with undo/redo (`Ctrl + Z` / `Ctrl + Y`)

### File Operations

- Create `NewFile.txt` / `NewFolder` from the canvas
- Move items between folders by drag and drop
- Move several selected items together in a single drag
- Copy OS files and folders by dropping them onto folder nodes
- Copy selected items with `Ctrl + C` / `Ctrl + V`
- Watch external file and folder changes under the root and refresh changed folders automatically
- Export/import a Mycel Archive Markdown file
- Inline rename with F2 and Enter confirmation
- Context menus for refresh, delete, color, creation, and open actions
- Undo and redo with `Ctrl + Z` / `Ctrl + Y`, also available from the Edit menu and toolbar. Covers moves, reorders, renames, creation, deletion, paste, import, links, colors, collapse, and preview state
- Deleted items are moved to `.mycel/trash` so a delete can be reverted with undo (the trash is cleared on startup and shutdown)
- Run a Go source file as a script from the context menu (`go run`), with the output shown in a window
- Pipelines: link an input file to a script (`.py`/`.go`) and run it from the script's context menu to produce an output file (`runner script <input> <output>`); the output is auto-created and linked if absent
- "Open with…" in the file context menu to pick the application (the native Open-with dialog on Windows)
- Reopen a recently opened folder from File > Open Recent

### Preview And Editing

- Preview pane for text, HTML, Markdown, CSV, images, and videos
- Thumbnail previews of the first page of PDFs and the cover of EPUBs, cached under `.mycel/thumbnails` and generated when the preview is opened
- Inline previews for text and Markdown files, showing up to 200 lines so a taller frame reveals more text
- Text and other plain preview frames can be freely resized on both axes by dragging the lower-right corner; image, PDF, and EPUB frames keep their source aspect ratio
- Select and copy text inside previews
- Lightweight preview frames for images, PDFs, and other files
- Large-file safe: images are decoded downscaled for previews, and images with an extreme pixel count (decompression bombs) are skipped with a placeholder
- Plain-text edit mode for the selected editable file
- Preview pane placement can be switched between left, right, and bottom
- Clear preview/edit mode indicators with labels, background color, and borders
- Automatic saving, `Ctrl + S` to save, and `Esc` to return to preview mode
- Shared persistent preview/edit font size changed with `Ctrl + mouse wheel`
- Preview/edit text size can also be changed with `Ctrl + +`, `Ctrl + -`, `Ctrl + 0`, or trackpad pinch on the preview/edit pane
- Image previews open at original size unless their longest side exceeds 460 px, in which case they are scaled down to 460 px
- File-watcher refresh is paused while editing so focus and edit mode are preserved
- Rename by putting `Subject: filename.txt` on the first line
- Lines starting with `Subject:` or `Key:` are hidden from inline previews

### Persisted State

Mycel writes local metadata into a `.mycel` directory under the opened root folder.

| File | Contents |
| --- | --- |
| `.mycel/order.json` | Custom ordering |
| `.mycel/colors.json` | User-assigned node colors |
| `.mycel/previews.json` | Preview open/closed state and size |
| `.mycel/collapsed.json` | Folder collapse state |
| `.mycel/links.json` | File-to-file links |
| `.mycel/view.json` | Canvas view, window size, maximized state, full-screen state, and display mode (tree/board) |
| `.mycel/boards/<name>.json` | Board patterns (card positions, hidden state, per-pattern view) |

In `--no-mycel` mode, Mycel does not load or create these files. Reordering, node color changes, file-to-file linking, folder collapse restoration, and persisted view/window restoration are disabled in this mode.

Recent root-folder history is saved in the application settings. Mycel stores normalized absolute paths, removes duplicates, and keeps up to 20 entries.

### Archive Export And Import

The toolbar `Export` action writes the opened root folder as a Mycel Archive Markdown file.

- Text files supported by Mycel are embedded into one Markdown file.
- Lines in text bodies that start with `` ``` `` or `\` are escaped so they can be restored exactly.
- Binary files such as images, PDFs, videos, and Office documents are copied into a sibling `.assets` folder while preserving their relative paths.
- The Markdown file records the binary references, ordering, colors, preview state, collapsed folders, and file-to-file links.

The toolbar `Import` action reads a Mycel Archive Markdown file into a selected destination folder. Existing files are not overwritten; conflicting files are skipped and reported.

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
| + / - | Zoom in or out |
| F5 | Reload the whole map |
| F11 | Maximize or restore the window |
| Toolbar Theme | Switch between light and dark themes |
| ? | Show the cheat sheet |

On macOS, two-finger trackpad slide pans the canvas while trackpad pinch remains zoom. Windows and Linux keep the existing wheel behavior.

### Selection And Navigation

| Operation | Action |
| --- | --- |
| Left-click a node | Select it |
| Shift + left-click a node | Select the continuous visible range from the last selected node |
| Tab | Move to the next visible item at the same level |
| Shift + Tab | Move to the previous visible item at the same level |
| Up / Down | Move to the previous or next item in the same folder |
| Left | Move to the parent folder |
| Right | Move to the first linked target file, or the first item inside the selected folder |
| F2 with one file or folder selected | Rename it inline |
| O | Open the selected item with the OS default application |

### File And Folder Operations

| Operation | Action |
| --- | --- |
| Double-click a folder | Collapse or expand that folder |
| N | Create `NewFile.txt` in the selected folder or selected file's folder |
| Shift + N | Create `NewFolder` in the selected folder |
| Double-click a file | Show or hide its preview |
| Drag a file or folder node | Reorder it inside the same folder |
| Drag a file or folder node onto a folder | Move it into that folder |
| Drop OS files or folders onto a folder node | Copy them into that folder |
| Ctrl + C / Ctrl + V | Copy selected items into their parent folders |
| D | Ask before deleting the selected files or folders |

### Preview And Editor

| Operation | Action |
| --- | --- |
| Enter | Toggle selected file previews or selected folder collapse |
| Drag the lower-right corner of a preview | Resize the preview (text frames resize freely on both axes; image/PDF/EPUB keep their aspect ratio) |
| Select preview text and press Ctrl + C | Copy text |
| Click the preview pane for an editable selected file | Enter plain-text edit mode |
| E | Edit the selected file as plain text |
| Ctrl + E | Show or hide the preview pane |
| Toolbar Preview Place | Move the preview pane to the left, right, or bottom |
| Ctrl + S in edit mode | Save and apply `Subject:` file-name changes |
| Esc in edit mode | Save and return to preview mode |
| Ctrl + mouse wheel in preview/edit mode | Change text font size |
| Ctrl + + / Ctrl + - in preview/edit mode | Increase or decrease text font size |
| Ctrl + 0 in preview/edit mode | Reset text font size |
| Trackpad pinch on the preview/edit pane | Change text font size |

### File Links

| Operation | Action |
| --- | --- |
| Drop a file on or near the right edge of another file | Link the files |
| Right-click a linked target file and choose `関連を解除` | Restore its original folder connection |

## Context Menus

### Single File

- Open/close preview
- Remove link
- Edit
- Rename
- Copy
- Delete
- Color
- Open

### Multiple Files

- Open/close preview
- Copy
- Delete

### Single Folder

- Collapse or expand
- Create file
- Create folder
- Rename
- Copy
- Paste
- Delete
- Color
- Open

### Mixed Files And Folders

- Open/close preview
- Copy
- Delete

## Build

Qt 6 Widgets, Multimedia, and MultimediaWidgets are required.

macOS:

```sh
./scripts/build-mac.sh
```

On macOS, the build output is `build/Mycel.app`.

Linux:

```sh
./scripts/build-linux.sh
```

Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

On Windows, the script can auto-detect Qt installed under `C:\Qt`. It prefers an MSVC Qt kit such as `C:\Qt\6.x\msvc2022_64` when available, imports the Visual Studio C++ environment, and builds a GUI-subsystem executable that does not open a terminal window at startup. After building, it runs `windeployqt` to deploy the Qt runtime files. With an MSVC kit, the default build directory is `build-windows-msvc`.

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

### Linux Package (.deb)

Build and create a `.deb` package in one step (requires `dpkg-dev`):

```sh
./scripts/package-linux.sh
```

The package is written to `dist/`. Installing it places `mycel` in `/usr/bin` and registers the desktop entry and icon for the application menu.

```sh
sudo apt install ./dist/mycel_<version>_amd64.deb
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

## License

Mycel is licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE).
