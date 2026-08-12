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

## Answering Policy

- 設定、仕様、既存実装、シナリオ意図に関わる回答は、必ず該当するローカルデータ、設計書、コード、会話ファイルを確認してから行う。
- 未確認の内容を推測で断定しない。推測や仮説を出す場合は、その根拠と未確認であることを明示する。
- 資料を読まずに、新しい設定・意図・因果関係をもっともらしく作らない。

## Google Sheets Data

- `Objects`、`Stages`、`Enemies` など Google Sheets から読み込むデータは、`data/google_sheet_source.cfg` が指すスプレッドシート側を最新かつ正のデータとする。
- `.tmp_objects.csv`、`.tmp_stages.csv`、`.tmp_enemies.csv` などリポジトリ直下の `.tmp_*.csv` は、オフライン起動やフォールバック用のローカルスナップショットであり、内容が古い可能性がある。最新値の確認、仕様判断、回答の根拠として単独では使用しない。
- Google Sheets 由来データについて回答または編集する前に、可能な限り対象シートの現行データを直接取得し、列名と対象行を確認する。シートへ接続できない場合は、最新値を確認できていないことを明示し、ローカルスナップショットの値を現行値として断定しない。
- 実行時にどちらのデータが使われるかを説明する場合は、Google Sheets 読み込みが許可された起動経路と、`.tmp_*.csv` を使うローカル／フォールバック経路を区別する。

## Text and Scenario Docs

- ゲーム内イベントのメッセージウィンドウに表示する会話・地の文、紙芝居、エンディング文など、シナリオ本文を追加・更新する場合は、該当するローカルデータや設計書だけでなく、以下の Google Docs も同じ内容に更新する。
  - https://docs.google.com/document/d/12xGwm99F709SkqDlyVlm3iP2ad2Xj9N-DQJHwGZrpaU/edit?usp=drivesdk
- HUD、メニュー、ボタン、操作ヘルプ、プロンプト、トースト通知、ダンジョンログ、デバッグ表示などの UI テキストは、上記 Google Docs の更新対象外とする。
- Google Docs がタブ付き文書の場合は、該当するタブだけを更新し、無関係なタブや既存フォーマットを崩さない。
- Google Docs に新しいシナリオ本文タブを作成・追加する場合、個別の指示がなくても既存の同種タブに合わせて、少なくとも `ID`、`ゲーム側会話`（対応するローカルファイルと trigger）、`反映状況` を本文冒頭に記載する。既存タブを初めてゲームへ実装する場合も、これらの管理情報がなければ同時に追記する。
- Google Docs を更新した後は、可能な範囲で対象タブを読み返し、反映されたことを確認する。
- ゲーム内セリフは基本的に文末の句点「。」を付けない。同じ行で文を続ける場合など、読みやすさに必要な場合だけ使ってよい。
- Google Docs の本文で1行空きがある箇所を `.story` に反映する場合は、その区切りに `@wait small` を入れる。`.story` の構文上の空行とは区別し、docs 側の意図された余白だけをウェイト化する。
- 会話テキスト案を出す場合は、docs の記述形式に合わせて `キャラ名：セリフ` 形式で出力する。話者なしの演出行は地の文として書く。
- 会話テキスト案は Google Docs へ貼り付けやすいよう、余白は空白行だけで表現し、Markdown の箇条書き・引用・コードブロックなどの装飾を使わない。
- 会話テキスト案をユーザーへ提示するときは、コピーしやすいようコードブロックではなく通常本文で出力する。Google Docs 側で整えやすいよう、原則として空白行を入れずに連続行で出力し、必要な区切りの空白行は貼り付け後にユーザーが入れられる形にする。

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

### Auto-Reload Build Reuse

ゲームのビルド検証が必要な変更では、Codex 用ビルドを始める前に、現在の `dev_auto_reload.ps1` が生成したビルド結果を再利用できるか確認する。
自動リロードが動作中で、今回の保存を受けたビルドが完了する見込みなら、その完了を待ってから次のチェッカーを実行する。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command `
  "& .\tools\check_dev_build_result.ps1 -RequiredConfig Release -ChangedPath @('src\game\GameBase.cpp', 'src\game\Game.hpp')"
```

`-ChangedPath` には、今回変更した `src` 以下のビルド入力をすべて渡す。
チェッカーが終了コード 0 を返すのは、結果を生成したものと同じ自動リロードプロセスが現在も動作中で、構成が一致し、
ビルド前後および現在の `src` と `CMakeLists.txt` の内容ハッシュが一致し、記録された実行ファイルも同一である場合だけとする。
変更した `.cpp` は再コンパイル記録への存在も検証される。`.hpp` / `.h` / `.inl` を変更した場合は、チェッカーが返す
`recompiledSources` を追加で読み、影響を受ける主要な翻訳単位が実際に再コンパイルされていることを確認する。
これらを満たして終了コード 0 になった場合は、その結果を Codex のビルド検証として採用し、同じ構成の Codex 用ビルドを重ねて実行しない。

自動リロードが動いていない場合は、Codex が自分で後述の隔離ビルドを必ず実行する。
検証のためだけに自動リロードを起動してはならない。また、自動リロードが動いていても、結果が存在しない、失敗または古い、
入力・構成・実行ファイルが一致しない、自動リロードがブロック中で現在の変更をビルドしない、必要な再コンパイルを確認できない、
あるいは妥当な時間内に再利用可能な結果が得られない場合は、Codex 用の隔離ビルドへ切り替える。
`dev_build_status.txt` の `ready` だけでは成功根拠にしない。

自動リロードの機械可読な最新結果は `%LOCALAPPDATA%\MajoShovel\build-logs\dev-auto-reload\latest-result.json` に保存される。
ログは同じディレクトリの `last-success.log`、`last-failure.log`、および `failures` 内の直前 4 件だけを保持し、各ログは最大 4 MiB に制限する。
したがって自動リロードを長期間動かしてもログが無制限には蓄積しない。

Codex が `tools\build.ps1` または `build_game.bat` で検証ビルドを実行する場合、既定ではロック付きのスロットプールを使う。
出力先は `%LOCALAPPDATA%\MajoShovel\build-codex\pools\<SOURCE_HASH>\<BUILD_FLAVOR>\slot-N` になる。
同時に複数スレッドがビルドしても同じ slot は同時使用されず、スレッドが変わっても温まった slot を再利用できる。
通常の Codex 検証ビルドでは `-BuildDir` を指定せず、`-CodexBuildSlots 0` も使わず、必ずこのスロットプールを再利用する。
タスクごとの隔離や「念のため」という理由だけで新しい検証ディレクトリを作ってはならない。スロットのロック機構がビルド出力の競合を防ぐため、
新規ディレクトリは不要なフルビルドと検証時間の増加を招く。
明示的な `-BuildDir` を使ってよいのは、既存スロットの破損を確認した場合、または特定の generator・構成・既存ビルドツリーを再現するなど、
スロットプールでは満たせない具体的な検証要件がある場合だけとする。その場合は、実行前にスロットを再利用できない理由を明記する。
ただし、スロットが分離するのはビルド出力先だけであり、複数の Codex スレッドは同じソースツリーを共有する。
別スレッドが `Game.hpp` や `Game*.cpp` などを編集中にビルドすると、ビルド途中で入力ソースが変わったり、想定外の再コンパイルや一時的なコンパイルエラーが発生したりする。
一方、`dev_auto_reload.ps1` の `build-nopch` と Codex の既定スロットは出力先が分離されているため、再利用条件を満たせない場合は、
ソースの書き換え中でなければ同時にビルドしてよい。再利用可能な自動リロード結果を待っている間に、同じ構成の Codex 検証を先回りして開始しない。
検証ビルドを始める前に、実行中の `cmake.exe` / `MSBuild.exe` / `cl.exe` / `link.exe`、作業ツリーの更新状況、
および今回のビルド入力となる `src` と `CMakeLists.txt` の状態を確認する。
同じ共有ファイルを別スレッドが書き換えている最中と判断できる場合は、そのファイル更新が一段落するまで待つが、別出力先のビルド完了までは待たなくてよい。
並行ビルドを行う場合は、開始直前に少なくとも `src` と `CMakeLists.txt` のハッシュまたは更新時刻の一覧を記録し、終了後に同じ状態か照合する。
途中でビルド入力が変わっていた場合、その結果を現行ソースの検証成功として採用しない。編集が落ち着いてから、同じ温まったスロットで必要な検証だけを再実行する。
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

検証ビルド後は終了コードだけで成功扱いしない。今回変更した `.cpp`、または変更した `.hpp` の影響を受ける主要な翻訳単位が、
実際に再コンパイルされたことを必ず確認する。
確認方法は、ビルドログや `MajoShovel.dir\<Config>\*.tlog` に対象 `.cpp` のコンパイルが記録されていること、または対象 `.obj` の更新時刻が
対応するソースより新しいことを見る。特に Visual Studio generator / MSBuild のインクリメンタルビルドでは、`cmake --build` が成功しても
問題の翻訳単位が再コンパイルされていない場合があるため、対象 `.cpp` を触った修正ではこの確認を毎回行う。
対象が再コンパイルされたことを確認できない場合は、そのビルド結果を修正確認として報告しない。
同じ構成・同じ隔離出力先で対象が再コンパイルされるようにしてから再検証する。

PowerShell から別の PowerShell を起動して `tools\build.ps1` を実行した場合、スクリプト内の `return <非0値>` が
呼び出し元プロセスの終了コード 0 として見えることがある。ラッパーの出力が不自然に少ない、対象 `.obj` が更新されていない、
実行ファイルが更新されていない、という場合は、外側で得た `$LASTEXITCODE` が 0 でも成功扱いしない。
使用された隔離ビルドディレクトリに対して同じ構成の CMake ビルドを直接 `--verbose` 付きで実行し、出力をログへ保存して確認する。

```powershell
$buildDir = "<tools\build.ps1 が表示した output>"
$log = Join-Path $env:LOCALAPPDATA ("MajoShovel\build-logs\codex-build-direct-{0}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
cmake --build $buildDir --config Release --target MajoShovel --parallel 12 --verbose 2>&1 |
    Tee-Object -FilePath $log
$code = $LASTEXITCODE
exit $code
```

ビルドが timeout した場合は、同じビルドをすぐ再実行しない。
まず自分が起動した `cmake.exe` / `MSBuild.exe` / `cl.exe` / `link.exe` が残っているか確認し、残っていれば完了を待つか、
自分が起動したプロセスだけを止めてからログを確認する。
他スレッドや `dev_auto_reload.ps1` のビルドを巻き込む可能性があるため、起源不明の `cl.exe` / `MSBuild.exe` をまとめて kill しない。
同じ出力先に検証ビルドを重ねて起動しない。

ユーザーが `dev_auto_reload.ps1` や `build-nopch` 側のビルドエラーを報告した場合、Codex 用の Release ビルド成功だけで修正確認済みと判断しない。
同じ `-Config Debug` などで再利用可能な自動リロード結果があればそれを確認し、なければ報告された構成に合わせた検証ビルドを Codex 用の隔離出力先で実行する。
構成や出力先が違うと、再コンパイル対象・include 依存・Debug/Release 差で片方だけエラーが出ることがある。

### Parallel Builds

MSVC ビルドでは並列化を一層だけにする。このプロジェクトでは CMake/MSBuild の job 数で並列化する。

`dev_auto_reload.ps1` の結果を再利用できない場合、または別構成の検証が必要な場合は、既定の出力先が異なるため Codex 検証と並行実行してよい。
同時実行時は CPU・メモリ・ディスク競合で両方が遅くならないよう、Codex 側の `-Jobs` を単独実行時より下げる。
目安として自動リロード側が `-Jobs 12` なら Codex 側は `-Jobs 4`～`6` とし、論理 CPU 数や実際の負荷に合わせて調整する。
ただし、並行実行中にソースが更新された場合は、前述のビルド入力照合に従い、古い状態を含む結果を成功扱いしない。

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

`dev_auto_reload.ps1` の `build-nopch` と Codex の既定スロットを並行ビルドすることは許可する。
ただし、`-BuildDir` などで同じ出力先を指定したビルドを重ねて実行してはならない。
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
  - `GameInternal.hpp` 内の型、特に anonymous namespace にある内部型を `Game.hpp` のメンバ関数シグネチャやフィールド型に出さない。必要なら `Game.hpp` には標準型や既存公開型だけを置き、内部型を使う処理は各 `Game*.cpp` の helper に閉じ込める。
  - `Game.hpp` で `GameInternal.hpp` 内部型を forward declaration してはいけない。別の `majo::Type` を作ってしまい、不完全型や構成依存のビルドエラーの原因になる。

判断に迷う場合は、「その関数を直す時に何を触っている感覚か」で分類する。

- 商人、倉庫、拠点メニュー、拠点専用描画: `GameBase.cpp`
- 宝箱、敵ノード、ワープ、報酬、ダンジョン内処理: `GameDungeon.cpp`
- セーブ項目、保存形式、ロード復元: `GameSave.cpp`
- HUD、タイトル、ポーズ、ゲームオーバー、ステージクリアなど画面横断の描画: `GameRender.cpp`
- デバッグコマンド、エディタ、テスト画面: `GameDevTools.cpp`
- 起動、初期化、画面遷移、メイン update、データロード: `GameCore.cpp`

機能分割の目的はインクリメンタルビルド時間の短縮である。
挙動変更、責務再設計、リファクタリングは、単なるファイル分割と同じ変更に混ぜない。
