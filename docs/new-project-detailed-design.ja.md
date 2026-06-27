# Mycel 新規プロジェクト向け詳細設計

この文書は、Mycel を新規リポジトリで再設計・再実装するための詳細設計です。現行リポジトリを継続開発する前提ではなく、既存実装から得られた知見、必要機能、構造上の問題点を整理し、新規プロジェクトの設計入力とします。

## 1. 目的

Mycel は、フォルダとファイルをマインドマップ状に表示し、ファイル整理、閲覧、移動、プレビュー、編集を視覚的に行うデスクトップアプリです。

新規プロジェクトの目的は、次の基本操作を安定して提供することです。

- フォルダを開く
- ファイル/フォルダをノードとして表示する
- ノードを選択する
- ノードをドラッグして移動する
- ファイルを別フォルダへ移動する
- 同名衝突時に安全にリネームする
- 親子関係やリンクを線で表示する
- ファイル内容をプレビューする
- プレビュー領域からも対象ファイルを選択できる
- プレビューサイズを変更する
- 大きなマップでもスクロール、ズーム、再描画が破綻しない

## 2. 設計原則

- UI とファイル操作を分離する。
- 入力イベントとドラッグ状態を一箇所で管理する。
- ノード描画と操作確定を分離する。
- レイアウト計算と線描画を分離する。
- メタデータ更新をファイル移動処理から独立させる。
- fallback に依存しない。
- 自動テスト可能なコアロジックを先に作る。
- `MainWindow` や `NodeItem` に責務を集中させない。
- 通常アプリ本体に診断専用モードを混ぜない。

## 3. 全体構造設計

### 3.1 レイヤ構成

新規プロジェクトでは、次のレイヤに分割します。

```text
Application
  AppController
  MainWindow

Presentation
  BoardView
  NodeItem
  ConnectionLayer
  PreviewPane
  Toolbar / Menu

Interaction
  InteractionController
  DragController
  DropTargetResolver
  SelectionController
  PreviewResizeController

Domain
  ProjectModel
  TreeModel
  NodeId
  NodeState
  FileLink
  LayoutModel

Services
  FileOperationService
  MetadataService
  ThumbnailService
  PreviewContentService
  FileWatcherService

Infrastructure
  LocalFileSystem
  JsonMetadataStore
  QtFileWatcherAdapter
  QtPreviewAdapters
```

### 3.2 依存方向

依存方向は一方向にします。

```text
UI -> Interaction -> Domain -> Services -> Infrastructure
```

禁止する依存:

- `NodeItem` から直接ファイル移動しない。
- `BoardView` から直接ファイル移動しない。
- `ConnectionLayer` がファイルツリーを探索しない。
- `FileOperationService` が UI item を知らない。
- `MetadataService` が Qt の view/item を知らない。

## 4. 主要コンポーネント設計

## 4.1 `ProjectModel`

### 役割

アプリ全体のプロジェクト状態を表します。

### 保持する情報

- root path
- tree model
- selected node ids
- collapsed node ids
- preview opened node ids
- preview sizes
- file links
- display order
- user colors
- view state

### 注意点

path を直接 UI の識別子にしすぎると、リネームや移動で破綻しやすくなります。内部では `NodeId` を持ち、path は属性として扱う設計を優先します。

## 4.2 `TreeModel`

### 役割

ファイルシステム上のフォルダ/ファイル構造を domain model として保持します。

### ノード情報

```text
Node
  id
  parentId
  path
  name
  type: file | directory
  children
  state
```

### 主な操作

- scan root
- find by id
- find by path
- update path after move
- apply collapsed state
- apply display order

## 4.3 `FileOperationService`

### 役割

ファイル移動、コピー、削除、リネーム、衝突解決を担当します。

### 主な API

```text
planMove(sourceIds, targetDirectoryId) -> MovePlan
executeMove(plan) -> MoveResult
planRename(nodeId, newName) -> RenamePlan
executeRename(plan) -> RenameResult
```

### `MovePlan`

```text
MovePlan
  entries:
    sourcePath
    destinationPath
    conflictAction
    oldNodeId
    targetDirectoryId
  pathMappings:
    oldPath -> newPath
  errors
```

### 衝突解決

同名ファイルが移動先に存在する場合、上書きしません。

基本ルール:

```text
Note.txt
Note 2.txt
Note 3.txt
```

拡張子付きファイルでは complete suffix を維持します。

```text
archive.tar.gz
archive 2.tar.gz
```

### 禁止条件

- root 自身の移動
- 自分自身への移動
- ディレクトリを自分の子孫へ移動
- 存在しない source
- directory でない target への移動
- 権限不足

## 4.4 `MetadataService`

### 役割

`.mycel` 配下の状態ファイルを読み書きします。

### 対象

- collapsed
- preview open
- preview size
- colors
- links
- order
- view state

### path mapping

ファイル移動やリネーム後、`FileOperationService` が返す `oldPath -> newPath` mapping を受け取り、metadata を更新します。

## 4.5 `TreeLayoutEngine`

### 役割

表示に必要な座標、サイズ、線 geometry を生成します。

### 入力

```text
TreeModel
NodeVisualMetrics
CollapsedState
PreviewState
FileLinks
LayoutOptions
```

### 出力

```text
LayoutResult
  nodes:
    nodeId
    rect
    labelRect
    previewRect
    anchorPoints
  parentChildConnections:
    fromNodeId
    toNodeId
    path
    bounds
  fileLinks:
    fromNodeId
    toNodeId
    path
    bounds
  sceneBounds
```

### 方針

`ConnectionLayer` は `LayoutResult` を描画するだけにします。描画中に node 探索や layout 補正を行いません。

## 4.6 `BoardView`

### 役割

キャンバスの表示と入力イベントの入口です。

### 担当する操作

- pan
- zoom
- rubber band selection
- wheel scroll
- key event forwarding

### 担当しない操作

- drag session の状態管理
- ファイル移動
- drop target 確定
- metadata 更新

## 4.7 `NodeItem`

### 役割

ノードを描画し、hit test に必要な形状を提供します。

### 持つ状態

- node id
- visual rect
- selected
- hovered
- drop highlighted
- preview resize highlighted

### 持たない状態

- drag session
- filesystem path mutation
- move result
- metadata update

## 4.8 `InteractionController`

### 役割

UI 入力を操作意図へ変換します。

### 処理

- mouse press を click / drag candidate / resize candidate に分類
- mouse move を drag / resize / pan へ振り分け
- mouse release で controller へ確定を委譲
- keyboard 操作を selection / command へ変換

## 4.9 `DragController`

### 役割

ドラッグ操作を一元管理します。

### 状態

```text
DragSession
  sourceNodeIds
  pressScenePos
  currentScenePos
  thresholdReached
  currentDropTarget
  dragMode: move | link | reorder
```

### 流れ

1. press で drag candidate を作る
2. move が threshold を超えたら drag session 開始
3. move 中に visual offset と drop target を更新
4. release で drop target に応じた command を作る
5. command 実行後、model と view を更新する
6. cancel で visual state を戻す

## 4.10 `DropTargetResolver`

### 役割

現在の drag session と pointer 位置から drop target を決定します。

### 判定対象

- folder move target
- file link target
- reorder target
- no target

### 返却値

```text
DropTarget
  type
  targetNodeId
  reason
  visualHint
```

## 5. 機能設計

## 5.1 フォルダを開く

### 仕様

- ユーザーが root folder を選ぶ。
- root 以下を scan する。
- `.mycel` は表示対象から除外する。
- 初回表示時は root と子要素を layout する。

### エラー

- root が存在しない
- root が directory でない
- 権限不足

## 5.2 ノード表示

### 表示対象

- directory
- file
- preview open file
- collapsed directory

### 表示要素

- icon
- label
- selection frame
- hover frame
- preview body
- resize handle

## 5.3 選択

### 操作

- click: 単一選択
- Cmd/Ctrl click: 複数選択
- rubber band: 範囲選択
- keyboard: 上下左右移動
- preview click: 対象 file node を選択

### 方針

選択状態は `SelectionController` で管理し、`NodeItem` は selected visual を表示するだけにします。

## 5.4 ドラッグ移動

### 操作

- file node を掴む
- folder node 上へ移動
- release で file を target folder へ移動

### 方針

- drag session は `DragController` のみが持つ。
- `NodeItem` は位置を一時的に表示するだけ。
- 実移動は `FileOperationService` が行う。
- 移動結果で `TreeModel` と metadata を更新する。

## 5.5 同名衝突

### 仕様

移動先に同じ名前が存在する場合、上書きせずに連番リネームします。

例:

```text
Summary.md -> Summary 2.md
Summary 2.md が既にあれば Summary 3.md
```

### 注意点

複数ファイルを同時移動する場合、同一 batch 内の衝突も検出します。

## 5.6 ファイルリンク

### 仕様

file node から別 file node へ関連線を作る。

### 保存

`.mycel/links.json`

### 注意点

file link は tree layout とは別の connection として扱います。親子構造と混同しません。

## 5.7 親子線描画

### 仕様

directory と child node の間に線を描く。

### 方針

`TreeLayoutEngine` が path と bounds を生成し、`ConnectionLayer` が描画します。

## 5.8 プレビュー

### 対象

- text
- markdown
- image
- video
- YouTube link
- HTML

### 方針

preview widget はノード表示の一部ですが、selection や drag の判定を邪魔しないよう、イベントは `InteractionController` に通知できる構造にします。

## 5.9 プレビューサイズ変更

### 操作

- preview の resize handle を drag
- release で size を保存

### 保存

`.mycel/previews.json`

## 5.10 スクロールとズーム

### 方針

- scroll 中は描画品質を一時的に落としてもよい。
- layout 結果自体は変えない。
- connection path は scroll/zoom で消えてはいけない。

## 6. データ設計

## 6.1 `.mycel` ディレクトリ

保存候補:

```text
.mycel/
  collapsed.json
  previews.json
  colors.json
  links.json
  order.json
  view.json
```

## 6.2 path と id

新規設計では、UI 内部の主キーとして path だけに依存しない方針を推奨します。

候補:

- `NodeId` は scan 時に path から生成する安定 ID
- path 変更時は `NodeId` を維持できる場合は維持
- metadata は保存時には relative path を使う

## 7. 開発上の問題点

## 7.1 責務集中

現行実装では、`MainWindow` と `NodeItem` が多くの責務を持っています。

問題:

- 変更時の影響範囲が大きい
- テストが難しい
- 入力経路が読みづらい
- ファイル操作と UI 操作が密結合する

## 7.2 fallback の増加

問題:

- press/move/release の正規経路が分かりにくくなる
- 一つの不具合修正が別の基本操作を壊す
- 操作ログなしでは状態を追えなくなる

方針:

- fallback は原則禁止
- 必要な場合は controller に明示的な状態遷移として入れる

## 7.3 UI とファイル操作の密結合

問題:

- drag item から直接ファイル移動が発生する
- 移動失敗時の rollback が難しい
- metadata 更新漏れが起きやすい

方針:

- `FileOperationService` で plan と result を分ける
- UI は result を受けて model を更新する

## 7.4 線描画の不安定化

問題:

- 描画時に node 探索や補正を行うと、表示範囲や clip が壊れやすい
- scene bounds と connection bounds が一致しないと線が消える

方針:

- layout 時に connection geometry を確定する
- `ConnectionLayer` は描画専用にする

## 7.5 テスト戦略の混乱

問題:

- アプリ本体に `--self-test` のような診断モードを混ぜると、通常起動経路が複雑になる
- macOS の GUI 初期化や Qt plugin に依存し、テストが不安定になる

方針:

- コアロジックは通常の単体テスト target で検証する
- UI 操作は QTest などの専用 integration test target で検証する
- 本体 executable に診断専用分岐を入れない

## 8. テスト設計

## 8.1 コア単体テスト

対象:

- conflict rename
- move plan
- descendant move prohibition
- metadata path mapping
- tree scan
- layout result
- connection geometry bounds

## 8.2 UI 統合テスト

対象:

- click selection
- preview click selection
- mouse drag move
- preview resize
- rubber band selection
- scroll/zoom after connection drawing

## 8.3 回帰テスト

必須ケース:

- 新規空フォルダを開いて root が表示される
- 新規フォルダ作成後に表示される
- file を folder へ drag move できる
- 同名衝突時に rename される
- 移動後に線が残る
- preview resize が保存される

## 9. 新規プロジェクトでの推奨実装順序

1. domain model を作る
2. file operation service を作る
3. metadata service を作る
4. layout engine を作る
5. connection geometry を作る
6. Qt UI の最小 shell を作る
7. node rendering を作る
8. selection controller を作る
9. drag controller を作る
10. drop target resolver を作る
11. preview system を作る
12. file watcher を統合する
13. UI integration tests を追加する

## 10. 新規プロジェクトの初期ディレクトリ案

```text
src/
  app/
    AppController.*
    MainWindow.*
  domain/
    ProjectModel.*
    TreeModel.*
    NodeId.*
    FileLink.*
  services/
    FileOperationService.*
    MetadataService.*
    PreviewContentService.*
  layout/
    TreeLayoutEngine.*
    ConnectionGeometry.*
  interaction/
    InteractionController.*
    DragController.*
    DropTargetResolver.*
    SelectionController.*
  ui/
    BoardView.*
    NodeItem.*
    ConnectionLayer.*
    PreviewPane.*
  infrastructure/
    LocalFileSystem.*
    JsonMetadataStore.*
    QtFileWatcherAdapter.*

tests/
  domain/
  services/
  layout/
  interaction/
  ui/
```

## 11. 完了条件

新規プロジェクトで最初の安定版と判断する条件:

- file move が service の単体テストで保証されている
- drag move が UI integration test で保証されている
- line geometry が layout test で保証されている
- `NodeItem` が file operation を知らない
- `BoardView` が drop 確定を行わない
- `MainWindow` が巨大な処理本体になっていない
- fallback なしで基本操作が動く
