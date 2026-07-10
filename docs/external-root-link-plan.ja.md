# 外部ルートリンク（親フォルダ外の子ルート）設計メモ

子ルート（`.mycel` を持つルート）が**親ルートのフォルダ内に物理的に存在しなくても**、親のビューに扉ノードとしてリンクできるようにする拡張の設計メモ。[hierarchical-mycel-plan.ja.md](hierarchical-mycel-plan.ja.md) の「切り替え式」を前提に、その扉ノードを親フォルダ外へ張れるようにする。**0.8.1 で実装済み**（§8 の判断: リンク切れは非表示・色は対象外・扉での新規作成はブロック）。

## 1. 目的

- 別ドライブ・別ディレクトリツリーにあるプロジェクト（ルート）を、親ルートのマップ上に扉ノードとして配置し、Enter/ダブルクリックで行き来したい。
- 物理的なフォルダ移動をせずに、複数のルートを1つの親マップから組織化する。

## 2. 保存形式

親ルートの `.mycel/external-roots.json`（新規ファイル）：

```json
{
  "version": 1,
  "links": [
    { "dir": "", "target": "../other-project" },
    { "dir": "sub/notes", "target": "E:/projects/research" }
  ]
}
```

- `dir` : 扉ノードを表示するフォルダ（ルートからの相対キー。`""` = ルート直下）。`order.json` と同じキー規約。
- `target` : リンク先。同一ドライブなら相対パス（親ツリーごと移動しても追従）、別ドライブなら絶対パス。読み込み時にルート基準で解決。

## 3. リンクの作成・削除操作

| 操作 | UI | 挙動 |
| --- | --- | --- |
| 作成 | フォルダ（ルート含む）右クリック →「外部ルートをリンク…」 | フォルダ選択ダイアログ → 検証 → `external-roots.json` に追記 → rebuild |
| 削除 | 外部扉ノード右クリック →「外部ルートのリンクを解除」 | 記録のみ削除。**リンク先の実フォルダには一切触れない** |

作成時の検証：

- 現在のルート内のフォルダは不可（物理サブルートと重複するため。「子ルートにする」を案内）
- 同じフォルダへの重複リンクは不可
- 存在しないパスは不可

Undo 対応：`MetadataSnapshot` に `externalRootLinks` を追加し、作成・解除を通常の履歴（Ctrl+Z）に載せる。

## 4. 表示と切り替え

- **合成扉ノード**：`rebuild()` の `scanTree` 後、記録された各フォルダの子として注入する。`Node` に `isExternalRoot` フラグを追加し、`isSubRoot = true` も立てる（既存のバッジ描画・境界停止・Enter 切り替え・「このルートを開く」がそのまま効く）。名前はリンク先フォルダ名。
- **切り替え**：既存の `switchIntoSubRoot()` を再利用。リンク先の `.mycel/parent.json` に親を記録するため、切り替え後は既存の親ルートバーで戻れる。リンク先に `.mycel` がなければ切り替え時に生成される。
- **並び順**：親フォルダの `fileOrders_`（名前ベース）に載るので、ドラッグ・`Ctrl+↑↓` の並び替えは通常どおり可能。
- **リンク切れ**（ターゲット消失）：扉ノードは表示しない（`external-roots.json` の記録は残し、復活したら再表示）。※要検討: 欠損マーカー付きで表示する案もある。
- 扉ノードのコンテキストメニューは「このルートを開く」「外部ルートのリンクを解除」のみ（「親に統合」「子ルートにする」等は非表示）。

## 5. スキャン境界の規則（今回の確定事項）

**再スキャン・整合性チェックはアクティブルート内で完結させる。**

- 親ルートを開いている間、子ルート（物理サブルート・外部リンク先とも）の**内部**は走査しない：
  - `walkRealTree()` に境界停止（`stopAtSubRoots`）を追加。`.mycel` を持つ子フォルダで再帰を止める。現状は表示ツリーだけが境界停止で、**ファイル監視登録・起動スキャン・2分間隔のバックグラウンド reconcile・ハッシュシードはサブルート内部まで潜っている**ため、ここが挙動変更点（物理サブルートにも適用）。
  - サブルートのディレクトリ自体も監視対象に含めない（扉の削除・リネームは親フォルダ側の監視で検知できる）。
  - 外部リンク先はそもそも `rootPath_` 外なので、走査・監視・ハッシュの対象に自然に入らない。
- 子ルートを開いている間、親ルート側のチェックは行わない（現状も `parent.json` の読み取りと存在確認のみで、親の内容走査はない）。

## 6. ファイル操作の保護（外部扉ノード）

リンク先は実フォルダなので、誤操作でリンク先本体を壊さないようにガードする：

| 操作 | 扱い |
| --- | --- |
| 削除（`D` / Delete / 複数選択削除） | 対象外（削除カウントに含めない）。解除はメニューの「リンクを解除」で |
| 移動（ドラッグ / `Ctrl+←`） | 不可（`moveNode` 入口でブロック） |
| リネーム（`Enter` / `F2`） | 不可（`beginInlineRename` でブロック） |
| コピー（`Ctrl+C`） | 対象外（外部ツリー全体の物理コピーになるため） |
| 並び替え（ドラッグ / `Ctrl+↑↓`） | 可（親の `fileOrders_` のみ変更） |
| 新規作成（`N` 等、扉を選択中） | ブロック（親の `order.json`・trash に `../` キーが漏れるため。編集はルート切り替えで） |

## 7. 実装箇所の当たり（調査済み）

| 変更 | 場所 |
| --- | --- |
| `Node::isExternalRoot` 追加 | `tree_model.h:150`（Node 構造体） |
| `walkRealTree` 境界停止 | `tree_model.h:1001`。呼び出し元: `runStartupScan`（同 1056）、監視リセット（`main_window_filesystem.cpp:102`）、`loadMetadataAndReconcileHashCache`（`main_window_core.cpp:741`）。バックグラウンド reconcile は `runStartupScan` 経由（`main_window_filesystem.cpp:425`） |
| `external-roots.json` 読み書き | `main_window_persistence.cpp`（`order.json` 等と同型。ロードは `loadMetadataAndReconcileHashCache` / `completeDeferredStartup`） |
| 合成ノード注入 | `main_window_layout.cpp:194` `rebuild()` の `scanTree` 直後（折りたたみ中の親には注入せず `hiddenChildren` に加算） |
| 作成・解除ハンドラ | `main_window_core.cpp`（`makeFolderChildRoot` 周辺に並べる） |
| コンテキストメニュー | `main.cpp` `showContextMenuAt`（フォルダ分岐に「外部ルートをリンク…」、外部扉ノード分岐を新設） |
| 操作ガード | `moveNode` / `beginInlineRename` / `copySelectedItems` / `selectedDeletableItemCount` / `deleteSelectedItems` / `promoteSelectedNode` |
| Undo | `MetadataSnapshot`（`main_window.h:830`）+ `captureMetadataSnapshot` / `restoreMetadataSnapshot`（`main_window_persistence.cpp:80/87`） |

## 8. 未決事項

- リンク切れ扉の表示：非表示（推奨）か、欠損マーカー付き表示か。
- 扉ノードへの色付け：メタデータキーが `../` 相対になる問題があるため、初期実装では対象外とするか。
- 扉ノード選択中の `N`（リンク先内部への新規作成）を許すか。
- 横リンク（`links.json`）と外部扉ノードの接続は既存方針どおり対象外。
