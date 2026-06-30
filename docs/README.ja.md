# Mycel ドキュメント索引

このフォルダには、Mycel の開発予定と設計に関するドキュメントをまとめています。

## 一覧

| ドキュメント | 種別 | 概要 |
| --- | --- | --- |
| [development-plan.ja.md](development-plan.ja.md) | 機能予定 | 今後検討している操作・機能（フォルダ直下ソート など）の予定。 |
| [architecture-redesign-plan.ja.md](architecture-redesign-plan.ja.md) | 再設計計画（現行リポジトリ） | `main.cpp` に集中した責務を、入力・ドラッグ・ファイル操作・レイアウト・線描画へ段階的に分離する計画。Phase 0〜6。 |
| [new-project-detailed-design.ja.md](new-project-detailed-design.ja.md) | 詳細設計（新規プロジェクト） | 既存実装の知見を踏まえ、新規リポジトリで再実装する場合のレイヤ構成・コンポーネント設計・テスト設計。 |

## 2 つの設計ドキュメントの関係

- **architecture-redesign-plan.ja.md** は、現行の単一 `src/main.cpp` を壊さずに段階的にリファクタするための計画です。各 Phase はビルドが通り、基本操作が退化しないことを進行条件とします。
- **new-project-detailed-design.ja.md** は、同じ方針をゼロから作り直す場合の目標形です。両者はレイヤ分割（UI / Interaction / Domain / Services / Infrastructure）の考え方を共有します。
- どちらも、診断専用の `--self-test` 分岐を本体実行ファイルに混ぜない方針です（自動テストは別ターゲットで行う）。

## 現在の進捗（0.5.3 時点）

再設計計画のうち、次が着手済みです。

- Phase 1（ファイル操作層の分離）: `FileOperationService` を抽出。ファイル移動・コピー・同名衝突リネームを UI から分離し、old→new パスの対応を返す形に整理。
- Phase 2（レイアウト／線geometryの分離）: `TreeLayoutEngine` を抽出。座標・node bounds・親子線/リンク線のパスを生成元として一本化し、bounds 確保と `ConnectionLayer` の描画が同一パスを使うように統一。横連携（ファイルリンクの配置）もここに集約。
- Phase 3 の一部（入力・ドラッグの一本化）: カードのドラッグ操作の主導権を `NodeItem` の暗黙マウスグラブから `BoardView` へ移し、press / move / release を単一の所有者で扱うように変更。リリース取りこぼしとドラッグ中の再描画クラッシュを解消。さらにドラッグセッションを `DragController` に集約。
- Phase 3 の一部（drop先判定の分離）: フォルダ移動先／リンク先のヒットテストを `DropTargetResolver` に集約し、`MainWindow` から判定ロジックを除去。
- Phase 3 の一部（選択処理の一本化）: シーン選択の変更を `SelectionController` に集約し、`NodeItem` から直接の `clearSelection`/`setSelected` を除去。
- Phase 5（自動テストの整備・着手）: 純粋なファイル操作ロジックを `src/mycel_fileops.hpp` に分離し、GUI 非依存の `ctest` ターゲット（`tests/fileops_test.cpp`）を追加。衝突リネーム・移動・子孫移動禁止・同一dir no-op・複数同時移動の衝突を検証。`ctest --test-dir build` で実行。
- 安定化: クラウド同期フォルダで多発する待機中の再描画ループを抑制（表示構造が変わったときだけ再描画）。
- 操作履歴（Undo/Redo, 0.5.0）: メタデータ全体スナップショット＋ファイル移動列で、移動・名前変更・作成・削除・貼り付け・取り込み・関連付け・色・折り畳み・プレビュー状態を取り消し/やり直し可能に。削除は `.mycel/trash` への退避方式へ変更し、`Ctrl + Z` / `Ctrl + Y`・「編集」メニュー・ツールバーから操作。非 ASCII 名の移動例外、複数選択ドラッグ、右クリックでの選択枠消失、選択枠の再描画取り残しを修正。
- 作成操作の改善（0.5.1）: 作成後に新規項目へフォーカス移動。コンテキストメニューで作成先（同じ階層／下の階層）と横リンク作成を選択可能に。「同じ階層に作成」は選択項目の直後に挿入。
- スクリプト実行・パイプライン・種別アイコン（0.5.2）: `.go` の単体実行（`go run`）と、入力→スクリプト→出力を横リンクで繋ぐパイプライン実行（`.py`/`.go`）を `QProcess` で追加。ファイルアイコンを拡張子バッジ＋アクセント色で種別表示。大文字小文字のみの名前変更を修正。
- アプリ選択で開く・起動高速化（0.5.3）: ファイル右クリック「別のアプリで開く…」を追加（Windows は OS 標準の「開く方法を選択」ダイアログ、他OSはアプリ選択して起動）。QtWebEngine（Chromium）を HTML プレビュー初回時まで遅延初期化し、起動時の Chromium 子プロセス起動を回避してメモリ約115MB削減・コールド起動を短縮。

未着手の主な項目: `TreeLayoutEngine`（線 geometry/bounds）の単体テスト追加、キー処理の重複解消、`InteractionController` の分離、`NodeItem`・`MainWindow` の軽量化。
