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
