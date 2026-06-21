# Mycel

Mycel は、ディレクトリ構造をホワイトボード風のマインドマップとして可視化する C++/Qt 製ファイルブラウザです。

ファイルを縦方向の一覧として表示するのではなく、フォルダとファイルを接続されたノードとして配置します。プロジェクト構造、文書群、知識ベースを地図のように探索するためのツールです。

英語版は [README.md](README.md) です。

現在のバージョンは 0.2.0 です。

## 主な機能

- ホワイトボード風のキャンバス
- ディレクトリ構造をマインドマップとして表示
- キャンバス上からフォルダとファイルを作成
- ドラッグ&ドロップでフォルダ間移動
- ファイルとフォルダを混在した順番で並び替え
- `.mycel/order.json` に並び順を保存
- `.mycel/colors.json` にユーザー指定色を保存
- フォルダの折りたたみと展開
- テキストと Markdown のインラインプレビュー
- Markdown のレンダリングと折り返し表示
- 複数選択、複数プレビュー開閉、複数ファイル削除
- 更新、名前変更、削除、色設定、作成、開くためのコンテキストメニュー
- タッチパッドのピンチズーム
- Ctrl + マウスホイールズーム
- 空白キャンバスをドラッグして範囲を選択し、Enter でその範囲へズーム
- Ctrl + 0 による全体表示

## ビルド

OS 別のビルドスクリプトを使えます。

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

Windows では、スクリプトが `C:\Qt` 配下にインストールされた Qt を自動検出できます。`C:\Qt\6.x\msvc2022_64` のような MSVC 版 Qt がある場合はそれを優先し、Visual Studio C++ 環境を取り込んで、起動時にターミナルウィンドウを開かない GUI subsystem の実行ファイルをビルドします。MSVC 版 Qt を使う場合の既定ビルドディレクトリは `build-windows-msvc` です。

手動で CMake を実行する場合:

```sh
cmake -S . -B build
cmake --build build
```

Qt 6 Widgets を優先して使用します。Qt 6 がない場合は Qt 5 Widgets にフォールバックします。

Qt Online Installer で Qt を入れていて `cmake` が `PATH` にない場合は、Qt 付属の CMake を使います。

```sh
/Users/fuk/Qt/Tools/CMake/CMake.app/Contents/bin/cmake -S . -B build -DCMAKE_PREFIX_PATH=/Users/fuk/Qt/6.11.1/macos
/Users/fuk/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build
```

### ビルドスクリプトのオプション

macOS/Linux スクリプトは以下の環境変数を参照します。

- `BUILD_DIR`: ビルドディレクトリ。既定値は `build`
- `BUILD_TYPE`: CMake のビルド種別。既定値は `Release`
- `CMAKE_BIN`: CMake 実行ファイル。既定値は `cmake`
- `CMAKE_PREFIX_PATH`: Qt が自動検出されない場合の Qt インストールパス

例:

```sh
CMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64 ./scripts/build-linux.sh
```

Windows スクリプトは引数を指定できます。

```powershell
.\scripts\build-windows.ps1 -BuildType Release -CMakePrefixPath "C:\Qt\6.x\msvc2022_64"
```

MinGW や特定の generator を使う場合は `-Generator` を指定します。

```powershell
.\scripts\build-windows.ps1 -Generator "Ninja" -CMakePrefixPath "C:\Qt\6.x\mingw_64"
```

## GitHub Actions

GitHub Actions 用の workflow を `.github/workflows/build.yml` に追加しています。

以下の環境で Mycel をビルドします。

- macOS
- Linux
- Windows

workflow では Qt 6 をインストールし、各OSのコンパイラ環境を設定した上で、OS別のビルドスクリプトを実行します。

## 起動

カレントディレクトリを開く:

```sh
./build/mycel
```

Windows MSVC ビルド:

```powershell
.\build-windows-msvc\mycel.exe
```

任意のディレクトリを開く:

```sh
./build/mycel /path/to/project
```

## マウス操作

- 何もない場所をドラッグ: 範囲選択
- 範囲選択後に Enter: その範囲へズーム
- Ctrl + 0: 全体表示
- Alt + 左ドラッグ、または中ボタンドラッグ: キャンバス移動
- タッチパッドのピンチ: ズーム
- Ctrl + マウスホイール: ズーム
- マウスホイール、またはタッチパッドスクロール: スクロール
- Shift + ファイルクリック: プレビューを開く/閉じる
- Shift + フォルダクリック: 折りたたみ/展開
- フォルダをダブルクリック: そのフォルダに新規ファイルを作成
- ファイルをダブルクリック: OS の既定アプリで開く
- ファイルまたはフォルダをドラッグ: 同じフォルダ内で並び替え
- ファイルまたはフォルダをフォルダへドロップ: そのフォルダへ移動
- プレビュー右下をドラッグ: プレビューサイズ変更
- ノードを右クリック: コンテキストメニューを表示

## コンテキストメニュー

単一ファイル:

- この項目を更新
- 全体を更新
- 名前を変更
- 削除
- 色を設定/クリア
- 開く

複数ファイル選択:

- 選択範囲を更新
- 全体を更新
- 選択ファイルのプレビューを開く
- 選択ファイルのプレビューを閉じる
- 選択ファイルを削除

フォルダ:

- この項目を更新
- 全体を更新
- 折りたたみ/展開
- フォルダを作成
- ファイルを作成
- フォルダを削除
- 色を設定/クリア
- 開く

## サンプル

`Sample` ディレクトリには、業務改革を進めるためのマインドマップサンプルが含まれています。

```sh
./build/mycel Sample
```

現状把握、課題構造、改革テーマ、施策、KPI、ロードマップ、コミュニケーション、リスク管理、会議メモなどを、サブフォルダと Markdown メモで整理しています。

## 補足

Mycel は、開いたルートフォルダ配下に `.mycel` ディレクトリを作成します。ここには並び順や色設定など、Mycel 用のローカルメタデータが保存されます。
