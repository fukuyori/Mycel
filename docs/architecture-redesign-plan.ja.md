# Mycel 全体構造再設計計画

この文書は、カード移動、ファイル移動、線描画、プレビュー操作などの基本操作を安定させるための再設計計画です。

現在の問題は、個別機能の不具合だけではなく、`MainWindow` と `NodeItem` に責務が集中しすぎていることにあります。今後は応急処置を増やすのではなく、入力、ドラッグ、ファイル操作、レイアウト、線描画を分離し、基本操作が壊れにくい構造へ移行します。

## 基本方針

- 症状ごとの fallback を追加しない。
- 必要以上の fallback、release 転送、重複 hover 判定は削除候補とする。
- `NodeItem` は表示とヒット判定に寄せる。
- `MainWindow` は各 controller/service の接続役に寄せる。
- ファイル操作は UI から分離し、単体で検査できるようにする。
- レイアウトと線 geometry は描画前に一貫して生成する。
- 基本操作は手動確認だけでなく、自動テストで保証する。

## 目標構造

### `NodeItem`

責務:

- ノードの描画
- `boundingRect` / `shape` / hit 判定
- 選択、hover、drag preview などの visual state 表示
- preview widget の最小限の保持

持たせない責務:

- ファイル移動
- drag session 管理
- drop target 判定
- metadata 更新
- reorder 確定

### `BoardView`

責務:

- mouse / keyboard / wheel event の入口
- canvas pan / zoom / rubber band selection
- 入力イベントを controller へ渡す

持たせない責務:

- fallback drag
- release 転送
- ファイル移動確定
- drop target の最終判断

### `InteractionController`

責務:

- 入力イベントの統合窓口
- click / drag / resize / context menu の振り分け
- `DragController` や preview resize への委譲

### `DragController`

責務:

- drag candidate の作成
- drag threshold 判定
- drag session の開始、更新、終了、キャンセル
- drag 中の visual update
- drop 時の command 実行

### `DropTargetResolver`

責務:

- folder drop target 判定
- file link target 判定
- drop 禁止条件の判定
- hover state に必要な target 情報の返却

### `FileOperationService`

責務:

- 移動可否判定
- 同名衝突時のリネーム名決定
- 単一/複数ファイル移動
- 自分自身または子孫への移動禁止
- old path / new path mapping の生成

`MainWindow` から直接 `std::filesystem::rename` を呼ばない構造にします。

### `TreeLayoutEngine`

責務:

- tree node の座標計算
- node bounds の計算
- 親子線 path の生成
- file link path の生成
- scene rect / connection bounds の計算

### `ConnectionLayer`

責務:

- `TreeLayoutEngine` が生成した geometry を描画する

持たせない責務:

- node 探索
- layout 補正
- 場当たり的な巨大 bounding rect

## 開発フェーズ

## Phase 0: 現状凍結と棚卸し

目的: 応急処置を増やす前に、削るべき実装と残す仕様を分ける。

作業:

- fallback 的な実装を一覧化する。
- 最近追加した drag 補助、release 転送、hover 管理、debug 分岐を分類する。
- 削除候補、維持候補、再実装候補を分ける。
- 基本操作の仕様を短く固定する。

完了条件:

- 削除対象リストがある。
- 維持する基本操作仕様がある。
- 以後、未分類の fallback を追加しない。

## Phase 1: ファイル操作層の分離

目的: ファイル移動を UI から切り離す。

作業:

- `FileOperationService` を新設する。
- `MovePlan` / `MoveResult` / `ConflictRenamePolicy` を定義する。
- 同名衝突時のリネーム規則を service 側へ移す。
- 複数移動、自分自身/子孫への移動禁止を service 側で扱う。
- metadata 更新に必要な old/new path mapping を返す。

完了条件:

- UI なしでファイル移動テストができる。
- 同名衝突、複数移動、子孫移動禁止がテストされている。
- `MainWindow` から直接 rename しない。

## Phase 2: レイアウトと線 geometry の分離

目的: 線が消える、表示範囲が壊れる、空フォルダが見えない問題を構造的に解消する。

作業:

- `TreeLayoutEngine` を新設する。
- node 座標、node bounds、connection path、scene bounds を一括生成する。
- `ConnectionLayer` は生成済み path を描画するだけにする。
- file link の配置補正を layout engine 側へ移す。

完了条件:

- 空フォルダ、新規フォルダ、深いツリーで layout が破綻しない。
- connection bounds が巨大化しない。
- 線 geometry が UI なしでテストできる。

## Phase 3: 入力とドラッグ制御の一本化

目的: 「掴めない」「release が来ない」「View 側 fallback が必要になる」状態をなくす。

作業:

- `InteractionController` を新設する。
- `DragController` を新設する。
- mouse press / move / release を `BoardView -> InteractionController -> DragController` に集約する。
- `NodeItem` から drag session 状態を外す。
- `DropTargetResolver` で drop target 判定を統一する。

完了条件:

- drag session が一箇所で管理される。
- View 側 release 転送が不要になる。
- QTest で mouse press / move / release を送ってファイル移動できる。

## Phase 4: `NodeItem` と `MainWindow` の軽量化

目的: 変更時の影響範囲を小さくする。

作業:

- `NodeItem` から移動確定、drop 判定、metadata 更新を削除する。
- `MainWindow` からファイル操作、layout 計算、drag 確定処理を外す。
- `MainWindow` は controller/service の生成、接続、結果反映を担当する。

完了条件:

- `NodeItem` は visual state 中心になる。
- `MainWindow` に基本操作の詳細ロジックが集中していない。

## Phase 5: 基本操作テストの整備

目的: 基本操作が壊れたら即検出できるようにする。

必要なテスト:

- 空フォルダを開くとルートが表示される。
- 新規フォルダ作成後に表示される。
- カードをクリック選択できる。
- preview 部分クリックでも選択できる。
- カードをドラッグで掴める。
- ファイルをフォルダへ移動できる。
- 同名衝突時に `name 2.ext` へリネームされる。
- 移動後に線が残る。
- preview resize ができる。
- スクロール後も線が消えない。

完了条件:

- `ctest` で基本操作の回帰を検出できる。
- 手動確認だけに依存しない。

## Phase 6: 応急実装の削除

目的: 再設計後に不要になった補助経路を消し、構造を単純にする。

削除候補:

- View 側 fallback drag
- release 転送
- 重複 hover 判定
- debug のためだけの分岐
- 巨大 bounding rect
- 生ポインタ hover 状態
- `MainWindow` 内の直接ファイル移動処理
- `NodeItem` 内の drag 確定処理

完了条件:

- fallback なしで基本操作が動く。
- 基本操作の流れがコード上で追いやすい。
- Phase 5 のテストが通る。

## 推奨順序

1. Phase 0: 現状凍結と棚卸し
2. Phase 1: ファイル操作層の分離
3. Phase 2: レイアウトと線 geometry の分離
4. Phase 3: 入力とドラッグ制御の一本化
5. Phase 4: `NodeItem` と `MainWindow` の軽量化
6. Phase 5: 基本操作テストの整備
7. Phase 6: 応急実装の削除

## 開発ゲート

各 Phase は、次の条件を満たすまで次へ進めません。

- ビルドが通る。
- 既存の基本操作が退化していない。
- 新しく分離した責務に対応するテストがある。
- fallback を増やしていない。
- 削除予定コードが増えていない。

## 判断基準

次の状態なら、再設計は不十分です。

- drag の状態が複数箇所にある。
- release が来ないことを View 側 fallback で補っている。
- ファイル移動が UI item から直接実行されている。
- 線描画が描画時に node を探して補正している。
- debug ログなしでは基本操作の流れを追えない。

次の状態なら、次の開発へ進めます。

- 入力、drag、drop 判定、ファイル操作、layout、描画が分離されている。
- 基本操作の自動テストがある。
- fallback なしでドラッグ移動が動く。
- `NodeItem` と `MainWindow` の責務が小さくなっている。
