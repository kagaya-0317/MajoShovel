# Story Event Specification

## 共通コマンド

### `@story_phone <kind>`

電話SEを再生し、SEの想定尺が終わるまでストーリー進行を待機する。画面上のポップアップなど、追加の表示演出は行わない。

`kind` は以下を指定する。

- `incoming`: 電話がかかってくる。SE は `se.story.phone.incoming`。
- `outgoing`: 電話をかける。SE は `se.story.phone.outgoing`。
- `hangup`: 電話を切る。SE は `se.story.phone.hangup`。

導入位置は、通話の台詞へ入る直前に `incoming` または `outgoing`、通話終了台詞の直後かつ `@portrait_hide monica` の直前に `hangup` を置く。

### `@story_shake <profile>`

画面揺れ演出を再生し、演出が終わるまでストーリー進行を待機する。

`profile` は以下を指定する。

- `small`: 短い揺れ。SE は鳴らさない。
- `strong`: 地面が揺れる演出。SE は `se.story.rumble`。
- `boss`: 強めの揺れ。SE は `se.story.rumble`。

地面が揺れてゴゴゴ…という演出を入れる場合は、演出行 `※地面が揺れる` の直後に `@story_shake strong` を置く。`@story_shake` 自体が待機するため、直後に揺れ待ち用の `@wait small` は置かない。

### `@dungeon_boss_spawn <mode> [seconds]`

ダンジョン中のボス前ストーリーで、現在ステージのボスを生成し、登場演出が終わるまでストーリー進行を待機する。

`mode` は以下を指定する。

- `emerge` / `ground_emerge`: 地面から出現する。土煙、地面破壊、画面揺れを伴う。
- `walk_in`: 画面奥の外側から、ボスの既存歩行グラフィックでゆっくり歩いてくる。`seconds` 省略時は `3.2` 秒。

`walk_in` は演出中だけボスを無敵・AI停止の spawn presentation 状態に置き、演出終了後は同じボス実体がそのまま戦闘に入る。

### `@base_return_scene begin`

帰還後の拠点会話で使う共通開始演出を挿入する。

- ルネを坑道出口から戻った位置へ配置する。
- 村長とモニカを会話用の開始位置へずらす。
- 少し待ってから、ルネを右上方向へ歩かせる。

`opening_base_intro`、ステージクリア後の拠点会話など、ルネが坑道から戻って村長・モニカと話すイベントの冒頭に置く。

### `@base_return_scene end`

帰還後の拠点会話で使う共通終了演出を挿入する。

- 黒フェードアウトする。
- 村長とモニカの会話用位置ずらしを解除する。
- 黒フェードインする。

イベント本編の会話が終わった後、システム説明や報酬説明の narration へ入る直前に置く。

### `@base_chicory_figure8 [seconds]`

拠点ストーリー中、ルネの帽子付近からチコリの小さな光を出し、ルネの周囲を八の字に飛ばす。

- `seconds`: 演出時間。省略時は `2.2` 秒。

演出中はストーリー進行を待機する。チコリは小さな発光体として描画し、地面に影を落とす。

### `@base_ring_demo open <ring_count> <item_object_id> [item_ring=<ring_number>] [seconds]`

拠点ストーリー中、ルネを中心にスペルリングのデモ表示を開く。

- `ring_count`: 表示するリング数。`1` から `3`。
- `item_object_id`: 見せ用アイテム。`item_apple` など。
- `item_ring`: 見せ用アイテムを乗せるリング番号。`1` から `3`。省略時は既存互換のため `2`。
- `seconds`: 開く演出時間。省略時は `1.15` 秒。

リング1には現在装備中のリング1アイテムを通常通り表示する。見せ用アイテムは保存データ、インベントリ、実際のスペルリングには追加しない。`open` 時にダンジョンのリング出現演出と同じ `se.ring.appear` を鳴らす。`open` 完了後も表示は維持され、`close` で閉じる。

### `@base_ring_demo close [seconds]`

拠点ストーリー中のスペルリングデモ表示を閉じる。

- `seconds`: 閉じる演出時間。省略時は `0.55` 秒。

### `@story_jingle <profile|se_id> [fallback_seconds]`

ストーリー中にBGMを一時的に下げ、SE扱いのジングルを鳴らす。コマンド自体は待機しない。

- `ring_unlock`: スペルリング解禁用ジングル `se.ring.unlock.jingle`。
- `level_up`: レベルアップジングル。
- `game_over`: ゲームオーバージングル。
- `se_id`: `se.` で始まる任意のSE ID。`fallback_seconds` 省略時は `1.28` 秒。

### `@dungeon_boss_explode_escape <sprite_key> [seconds]`

ダンジョン内のボス後ストーリー中、撃破済みボスを演出用に再表示し、爆発後に別 sprite へ変化させて逃走させる。

- `sprite_key`: 逃走時に表示する sprite 種別。`crab_dish` は `assets/enemies/story_crab_dish.png` を表示する。
- `seconds`: 演出全体の時間。省略時は `1.8` 秒。

再表示するボスは実際の敵として再スポーンせず、保存データ、敵リスト、ドロップ、撃破フローには影響しない。逃走 sprite は向き差分を持たない場合、移動方向に関係なく同じ画像を使う。

ラスボス初回のアストラグナ救出時は、共通撃破演出と `boss_after` 用の再配置フェードを挟まず、救出した戦闘画面のまま `boss_after:stage_03_star_core` を開始する。会話完了後は白フェードアウト/白フェードインでメインエンディング紙芝居へ移行する。
