# バージョン更新チェックリスト

バージョン番号を変更するときに更新するファイルの一覧です。バージョン番号を直接書いているのは次の 4 ファイルだけで、パッケージスクリプトはすべて `CMakeLists.txt` から読み取ります。

## 直接書き換えるファイル

| ファイル | 箇所 | 内容 |
| --- | --- | --- |
| `CMakeLists.txt` | 3 行目付近 `project(Mycel VERSION x.y.z LANGUAGES CXX)` | 唯一の正本。`MYCEL_VERSION`（アプリ内表示）、macOS バンドルのバージョン、Linux パッケージ名、Windows インストーラー名はここから導出される |
| `README.md` | 冒頭 `- Current version: x.y.z` | 英語版 README の現在バージョン |
| `README.ja.md` | 冒頭 `- 現在のバージョン: x.y.z` | 日本語版 README の現在バージョン |
| `CHANGELOG.md` | 先頭の `## 未リリース` 見出し | 見出しを `## x.y.z` に置き換える。未リリース節が無い場合は新しい節を追加する |

## あわせて確認するもの

- `README.md` / `README.ja.md` の機能一覧・操作一覧・`.mycel` 配下ファイル一覧に、そのバージョンで追加した機能の記述があるか
- `docs/README.ja.md` の索引に、そのバージョンで追加・確定した設計ドキュメントがあれば反映されているか

## バージョンを自動で読み取るもの（編集不要）

- `scripts/package-windows-inno.ps1`（`Read-AppVersion`）
- `scripts/package-mac.sh`（`APP_VERSION`）
- `scripts/package-linux.sh`（`APP_VERSION`）

## 確認コマンド

置き換え漏れがないか、旧バージョン文字列を検索する（ビルド出力は除外）。`CHANGELOG.md` の過去の見出しだけが残れば正常。

```powershell
git grep -n "<旧バージョン>" -- ':!build-*'
```

最後にビルドスクリプトを実行して CMake を再構成し、`CMakeCache.txt` の `CMAKE_PROJECT_VERSION` が新しい値になっていることを確認する。
