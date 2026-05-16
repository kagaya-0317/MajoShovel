# Repository Agent Instructions

このリポジトリで作業するエージェント向けのルールです。

## Encoding

- 既存ファイルのエンコードを勝手に変更しない。
- 新規作成・編集したテキストファイルは UTF-8 BOM 付きで保存する。
- コード内の日本語コメント・文字列が文字化けしないよう、編集後に必要なら BOM を確認する。

## Game.cpp Split Policy

`src/game/Game.cpp` の実装は機能別の `Game*.cpp` に分割している。
新しい `Game::` メソッドや既存メソッドの移動は、以下の置き場所ルールに従う。

- `GameCore.cpp`
  - ゲーム全体の入口と制御。
  - `initialize`, `update`, 画面遷移、ワールド初期化、データロード、hot reload。

- `GameBase.cpp`
  - 拠点内で完結する機能。
  - 商人、倉庫、加工、強化、指輪工房、本棚、拠点 UI の update/render。
  - 拠点専用の描画関数は、汎用 render ではなくここに置く。

- `GameDungeon.cpp`
  - ダンジョン中の状態と生成。
  - ダンジョン生成、宝箱、報酬、敵ノード、ワープ、リトライ、ボス、足元エフェクト、リング装備 FX。

- `GameSave.cpp`
  - セーブデータの読み書き。
  - 保存形式、保存先パス、ロード復元処理。

- `GameRender.cpp`
  - 画面全体にまたがる描画と HUD。
  - タイトル、画面遷移、リング画面、ポーズ、ゲームオーバー、ステージクリア、HUD、ダンジョンログ。

- `GameDevTools.cpp`
  - 開発用機能。
  - base edit、object image scale edit、enemy test、debug command、debug overlay。

- `GameInternal.hpp`
  - 分割された `Game*.cpp` から共有する helper の置き場。
  - まずは使用する `.cpp` の anonymous namespace に helper を置く。
  - 2 ファイル以上で本当に共有が必要な helper だけ `GameInternal.hpp` に置く。
  - `GameInternal.hpp` を太らせると複数の `Game*.cpp` が再コンパイルされるため、追加は最小限にする。

判断に迷う場合は、「その関数を直す時に何を触っている感覚か」で分類する。

- 商人、倉庫、拠点メニュー、拠点専用描画: `GameBase.cpp`
- 宝箱、敵ノード、ワープ、報酬、ダンジョン内処理: `GameDungeon.cpp`
- セーブ項目、保存形式、ロード復元: `GameSave.cpp`
- HUD、タイトル、ポーズ、ゲームオーバー、ステージクリアなど画面横断の描画: `GameRender.cpp`
- デバッグコマンド、エディタ、テスト画面: `GameDevTools.cpp`
- 起動、初期化、画面遷移、メイン update、データロード: `GameCore.cpp`

機能分割の目的はインクリメンタルビルド時間の短縮である。
挙動変更、責務再設計、リファクタリングは、単なるファイル分割と同じ変更に混ぜない。
