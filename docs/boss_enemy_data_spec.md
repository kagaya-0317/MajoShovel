# ボス敵データ仕様

## 目的

ボス敵の基礎ステータスは Google Sheets の `Enemies` シートを正とする。コード側はシートが読めない場合の互換フォールバックだけを持ち、通常実行ではシート値に隠し倍率を掛けない。

## データの置き場

- Spreadsheet: `魔女採掘　データ`
- Sheet: `Enemies`
- ボス敵ID: `stardust_mole`, `junk_crab`, `astragna`, `star_vein_dragon`
- `HP`, `接触攻撃力`, `経験値`, `半径` は実戦で使う最終値として入力する。
- `敵特殊タグ` には `boss`, `boss_only`, `no_normal_spawn`, `unique` を必ず含める。
- 通常スポーン抽選から除外するため、ボス定義では `boss_only` と `no_normal_spawn` を併用する。

## 関連シート

### #特殊タグ

ボス定義で使うタグは `#特殊タグ` に登録する。

| タグ | 用途 |
| --- | --- |
| `boss` | ボス敵判定。演出、報酬、捕獲制限、通常敵との差分に使う。 |
| `boss_only` | ボス出現経路専用。通常出現には使わない。 |
| `no_normal_spawn` | 通常掘削スポーンや重み抽選から除外する。 |
| `terrain_boss` | 地形・部位ギミックを持つ大型ボス。現状はアストラグナ用。 |

`large` は `small` / `medium` と同じ size 排他グループに属する。`heavy` は weight グループなので `large` と併用できる。

### #敵挙動コード

`boss_sequence` をボス専用の挙動コードとして登録する。`Enemies.敵挙動コード` では `always:boss_sequence:pattern=...:0` の形で専用ボスシーケンスを指定する。

## 被ダメージ補正

- ボスへの通常攻撃ダメージは一律 `0.75` 倍にする。
- ボス弱点へ命中した攻撃ダメージは一律 `1.5` 倍にする。
- 弱点倍率は `Enemies` や `boss_sequence` params では指定しない。コード側の固定仕様として扱う。
- 弱点に命中した場合は通常 `0.75` 倍を重ねず、元ダメージに対して `1.5` 倍を適用する。
- ダメージ倍率適用後の小数は切り上げる。

## 現行ステータス

| ID | 名前 | HP | 接触攻撃力 | 接触ダメージ種別 | 移動速度 | 半径 | 経験値 | 敵AI | 敵特殊タグ |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | --- | --- |
| `stardust_mole` | 星くずモグラ | 1440 | 2 | `blunt` | 21 | 15 | 60 | `stationary` | `boss,boss_only,no_normal_spawn,unique` |
| `junk_crab` | 廃品殻獣ジャンクラブ | 1680 | 4 | `blunt` | 54 | 18 | 90 | `stationary` | `boss,boss_only,no_normal_spawn,unique,large,heavy` |
| `astragna` | 星封殻アストラグナ | 1 | 0 | `none` | 0 | 24 | 120 | `stationary` | `boss,boss_only,no_normal_spawn,unique,large,terrain_boss` |
| `star_vein_dragon` | 星脈竜 | 4000 | 18 | `magic` | 58 | 30 | 180 | `hover_chase` | `boss,boss_only,no_normal_spawn,unique,large,heavy,magic` |

## コード側の責務

- `EnemySystem::spawnBossAt` は `Enemies` から読み込んだ値をそのまま使う。
- ボスHPや経験値をコード側の固定倍率で増やさない。
- `applyFallbackBossDefinition` はシートが存在しない、または対象ボス行が読めない場合だけ使う互換経路とし、値は上記ステータスと揃える。
- 新しい専用ボス挙動を追加する場合は、まず `#敵挙動コード` にコードを登録し、`Enemies.敵挙動コード` の params で調整できる形にする。
- ボス被ダメージ倍率は `BossNormalIncomingDamageMultiplier` と `BossWeakPointIncomingDamageMultiplier` で一元管理する。

## 備考

- `astragna` 本体の `HP` は生存フラグ用の最小値として扱う。本体HPバーは表示せず、実戦でHPを持つのは外殻ブロック、封印パーツ、封印発射点だけにする。
- `star_vein_dragon` は現時点では基礎ボス定義と汎用挙動のみを持つ。専用ボスシーケンス実装後は `boss_sequence` に移行する。
