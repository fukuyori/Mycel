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

## 現在の進捗（0.4.1 時点）

再設計計画のうち、次が着手済みです。

- Phase 1（ファイル操作層の分離）: `FileOperationService` を抽出。ファイル移動・コピー・同名衝突リネームを UI から分離し、old→new パスの対応を返す形に整理。
- Phase 3 の一部（入力・ドラッグの一本化）: カードのドラッグ操作の主導権を `NodeItem` の暗黙マウスグラブから `BoardView` へ移し、press / move / release を単一の所有者で扱うように変更。リリース取りこぼしとドラッグ中の再描画クラッシュを解消。さらにドラッグセッションを `DragController` に集約。
- Phase 3 の一部（drop先判定の分離）: フォルダ移動先／リンク先のヒットテストを `DropTargetResolver` に集約し、`MainWindow` から判定ロジックを除去。
- 安定化: クラウド同期フォルダで多発する待機中の再描画ループを抑制（表示構造が変わったときだけ再描画）。

未着手の主な項目: `InteractionController` / `SelectionController` の分離（選択・キー処理の重複解消）、`NodeItem`・`MainWindow` の軽量化、レイアウト／線geometryの分離（`TreeLayoutEngine`）、基本操作の自動テスト（`ctest`）整備。
