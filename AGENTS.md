# Repository Agent Instructions

このリポジトリで作業するエージェント向けのルールです。

## Encoding

- 既存ファイルのエンコードを勝手に変更しない。
- 新規作成・編集したテキストファイルは UTF-8 BOM 付きで保存する。
- コード内の日本語コメント・文字列が文字化けしないよう、編集後に必要なら BOM を確認する。

## Implementation Policy

- とにかく綺麗に実装する。場当たり的な分岐やコピペで済ませず、後から読んだ時に意図と責務が分かる形にする。
- 既に実装済みで使える関数、型、データ構造、描画処理、UI 部品、ユーティリティがある場合は、新しく似たものを作る前にそれを優先して使う。
- 同じ意味の処理や定数を複数箇所に散らさない。共通化できるものは共通化し、仕様変更や調整時に個別で何箇所も直す必要が出ないようにする。
- ただし、共通化のためだけに責務の違う処理を無理にまとめない。実際に重複を減らし、保守しやすくなる範囲で抽象化する。
- 修正は既存の設計、命名、ファイル分割、ヘルパー配置に合わせる。新しい実装方針を持ち込む場合は、局所的な都合ではなく長期的な保守性を優先する。

## Text and Scenario Docs

- ゲーム内に新規テキスト、会話、紙芝居、エンディング文、シナリオ説明を追加・更新する場合は、該当するローカルデータや設計書だけでなく、以下の Google Docs も同じ内容に更新する。
  - https://docs.google.com/document/d/12xGwm99F709SkqDlyVlm3iP2ad2Xj9N-DQJHwGZrpaU/edit?usp=drivesdk
- Google Docs がタブ付き文書の場合は、該当するタブだけを更新し、無関係なタブや既存フォーマットを崩さない。
- Google Docs を更新した後は、可能な範囲で対象タブを読み返し、反映されたことを確認する。

## Build

通常開発では prebuilt SDL3 を使うビルド経路を使う。

```powershell
.\build_game.bat
```

または CMake preset を直接使う。

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

ビルド出力は Dropbox リポジトリ直下ではなく `%LOCALAPPDATA%\MajoShovel` 以下に置く。
Dropbox の同期ロックを避け、リビルドを速く保つため。

Codex が `tools\build.ps1` または `build_game.bat` で検証ビルドを実行する場合、既定ではロック付きのスロットプールを使う。
出力先は `%LOCALAPPDATA%\MajoShovel\build-codex\pools\<SOURCE_HASH>\<BUILD_FLAVOR>\slot-N` になる。
同時に複数スレッドがビルドしても同じ slot は同時使用されず、スレッドが変わっても温まった slot を再利用できる。
スロット数は既定で 4。変更する場合は `-CodexBuildSlots <数>` または環境変数 `MAJOSHOVEL_CODEX_BUILD_SLOTS` を使う。
`-CodexBuildSlots 0` を明示した場合だけ、従来どおり `%LOCALAPPDATA%\MajoShovel\build-codex\<CODEX_THREAD_ID>` を使う。
`dev_auto_reload.ps1` の `%LOCALAPPDATA%\MajoShovel\build-nopch` と衝突させないため、Codex 検証で
`-BuildDir` を指定する場合も `build-nopch` は使わない。

Codex 文脈では、`tools\build.ps1` は `sccache` または `clcache` と `ninja.exe` が見つかる場合、
`-CompilerCache Auto` で Ninja generator とコンパイラキャッシュを使う。見つからない場合は従来の Visual Studio generator に戻る。
通常開発の `build-nopch` では、既存の Visual Studio generator ビルドツリーと混ざらないよう自動では Ninja に切り替えない。
キャッシュを使わない確認をしたい場合は `-CompilerCache Off` を指定する。
明示的に固定する場合は `-Generator VisualStudio` または `-Generator Ninja` を使う。

### Codex Verification Builds

Codex が検証ビルドを実行する場合、ビルドは 10 分以上かかることがある前提で扱う。
短い timeout で様子見してから再実行する運用は禁止。最初から完了まで待てる timeout を設定する。
目安として 30 分以上、重い変更や初回ビルドでは 45〜60 分以上を使う。

検証ビルドはログを残して実行する。標準出力だけに頼らず、タイムアウトや中断後でも結果を確認できるようにする。
ログ保存先は `%LOCALAPPDATA%\MajoShovel\build-logs` を使う。

```powershell
$logDir = Join-Path $env:LOCALAPPDATA "MajoShovel\build-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir ("codex-build-{0}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss"))

Start-Transcript -Path $log
try {
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Jobs 12
    $code = $LASTEXITCODE
} finally {
    Stop-Transcript
}
exit $code
```

ビルドが timeout した場合は、同じビルドをすぐ再実行しない。
まず自分が起動した `cmake.exe` / `MSBuild.exe` / `cl.exe` / `link.exe` が残っているか確認し、残っていれば完了を待つか、
自分が起動したプロセスだけを止めてからログを確認する。
他スレッドや `dev_auto_reload.ps1` のビルドを巻き込む可能性があるため、起源不明の `cl.exe` / `MSBuild.exe` をまとめて kill しない。
同じ出力先に検証ビルドを重ねて起動しない。

### Parallel Builds

MSVC ビルドでは並列化を一層だけにする。このプロジェクトでは CMake/MSBuild の job 数で並列化する。

```powershell
.\tools\build.ps1 -Jobs 12
.\tools\dev_auto_reload.ps1 -Jobs 12
```

PowerShell の実行ポリシーにより `.\tools\build.ps1` や `.\tools\dev_auto_reload.ps1` の直接起動が拒否される場合がある。
その場合は PC 全体のポリシーを変更せず、プロセス単位の一時的な Bypass で同じスクリプトを実行する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Jobs 12
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\dev_auto_reload.ps1 -Jobs 12
```

`build_game.bat` と `★dev_auto_reload.bat` もこの方式で PowerShell スクリプトを呼び出している。

`cmake --build --parallel` または上記 `-Jobs` スクリプトを使う場合、`CMakeLists.txt` に MSVC `/MP` を追加しない。
外側のビルド並列と `/MP` を併用すると `cl.exe` の子プロセスが増えすぎ、以下で失敗することがある。

```text
cl : command line error D8040
```

この場合、リンク工程が完了せず `%LOCALAPPDATA%\MajoShovel\build-nopch\Release\MajoShovel.exe` が存在しないことがある。
これはビルド失敗または中断の結果であり、別個のランタイム問題ではない。

`dev_auto_reload.ps1` が同じ出力先をビルド中に、手動で `build.ps1` を重ねて実行しない。
ビルドを中断する必要がある場合は、自分が起動したビルドだけを止める。
すべての `cl.exe` / `MSBuild.exe` をまとめて kill すると、dev watcher 側のビルドも中断し、実行ファイルが欠けた状態になる。

通常ビルドは `external/SDL3-prebuilt` を使い、`external/SDL` はビルドしない。
prebuilt パッケージを置き換える必要がある場合のみ、vendored SDL source fallback を有効にする。

```powershell
cmake -S . -B "%LOCALAPPDATA%\MajoShovel\build-sdl-source" -DMAJOSHOVEL_VENDOR_SDL=ON -DMAJOSHOVEL_USE_PREBUILT_SDL=OFF
cmake --build "%LOCALAPPDATA%\MajoShovel\build-sdl-source" --config Release --target MajoShovel
```

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
