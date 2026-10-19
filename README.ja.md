<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/wordmark-dark.svg">
    <img src="assets/wordmark-light.svg" width="280" alt="just-desktops">
  </picture>
</h1>

<p><a href="README.md">English</a> · <b>日本語</b></p>

キーボードで切り替える、macOS のデスクトップ。小さな常駐プロセス 1 つ（約 8MB）で動きます。

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/overview-dark.svg">
    <img src="assets/overview-light.svg" width="720" alt="macOS の画面の模式図。下にデスクトップを切り替えるキー、中央にウィンドウスイッチャー、フォーカス中のウィンドウを囲む枠、右上にメニューバーの項目">
  </picture>
</p>

1. **デスクトップ。** `⌥1`、`⌥2`、`⌥3`… で、最大 10 個のデスクトップを切り替えます。Shift を足すと、今のウィンドウを連れて移動します。macOS の Spaces と SIP には触りません。
2. **ウィンドウスイッチャー。** `⌥` を押したまま Tab を押して、このデスクトップのウィンドウを選びます。表示は、リスト、大きなプレビュー、表示なし、から選べます。
3. **フォーカスの枠。** 入力中のウィンドウを、細い枠で囲みます。
4. **メニューバー。** 使うかどうかは選べます。このデスクトップのウィンドウを 1 つずつアイコンで並べ、デスクトップの番号を出します。

## インストール

```sh
mise use -g github:n-seiji/just-desktops
just-desktops
```

macOS がアクセシビリティの許可を求めます。許可は、ターミナルではなく `just-desktops` 自身に与えてください。ターミナルに許可を与えると、そこで起動したすべてのプログラムがアプリを操作できてしまいます。下の `mise run service` で起動すれば、許可を持つのはこのバイナリだけになります。終了すると、退避していたウィンドウはすべて画面に戻ります。

ログイン時に起動するには、リポジトリの中で次を実行します。

```sh
mise run service     # LaunchAgent を登録して起動する
mise run unservice   # 止めて、登録を消す
```

mise でアップグレードすると、バイナリのパスが変わります。`mise run service` をもう一度実行して、アクセシビリティを許可し直してください。

サービスのログは `~/Library/Logs/just-desktops.log` に出ます（以前の版は `/tmp/just-desktops.log` でした。`mise run service` で plist が書き直されるので、古いファイルは消してかまいません）。

**アプリ版は準備中です。** Developer ID で署名し、Apple の公証を通した `just-desktops.app` と、Homebrew の cask を用意しています。起動も設定もメニューバーからでき、ターミナルは要りません。詳しくは [docs/app.md](docs/app.md)（英語）を見てください。

## 使い始める

- 修飾キー（Option）を押したまま `1`、`2`、`3`、`Q`、`W`、`E` を押すと、デスクトップ 1 から 6 に移ります。
- Shift を足すと、今のウィンドウをそのデスクトップに移します。
- Option を押したまま Tab を押すと、このデスクトップのウィンドウを選べます。Option を離すと、そのウィンドウに移ります。
- 設定ファイルは `~/.config/just-desktops/config` です。項目はすべて省略できます。変更は再起動で反映されます。

## キー

`⌥` は基本の修飾キーです。`modifier` を設定しなければ Option です。

| キー | 動作 |
|---|---|
| `⌥1` `⌥2` `⌥3` `⌥Q` `⌥W` `⌥E` | デスクトップ 1 から 6 に切り替える |
| `⌥⇧` + 同じキー | 今のウィンドウを、そのデスクトップに移す |
| `⌥Tab` / `⌥⇧Tab` | 次 / 前のウィンドウを選ぶ。端まで行くと反対側に戻る |
| `⌥` を離す | 選んだウィンドウに移る |
| `Esc` | スイッチャーを閉じる |

## 設定

ファイル: `~/.config/just-desktops/config`。例は [config.example](config.example) にあります。

| 項目 | 値 | 既定値 | 動作 |
|---|---|---|---|
| `border` | `on`、`off` | `on` | フォーカス中のウィンドウを囲む枠。`off` なら枠のウィンドウを作らない |
| `switcher` | `list`、`square`、`off` | `list` | Tab スイッチャーの表示。`on` と未知の値は `list` になる |
| `menubar` | `on`（または `strip`）、`icon`、`off` | `off`（アプリ版は `on`） | メニューバーの項目。ウィンドウごとのアイコンか、小さなアプリアイコンだけか。アプリ版は常に項目を出すので、`off` は `icon` と同じ |
| `keys` | 数字か英字を 10 個まで | `1 2 3 q w e` | n 番目のキーがデスクトップ n |
| `modifier` | `opt`、`cmd`、`ctrl` | `opt` | すべてのホットキーの基本の修飾キー。[modifier](docs/workspaces.md#modifier) を参照 |
| `no_preview` | bundle id | — | square スイッチャーで画面を撮らないアプリ。複数行書ける |
| `<bundle id>` | `<n>`、`none` | — | アプリをデスクトップ n に固定する。`none` はどのデスクトップにも属さない |

```
switcher = square
menubar = on
com.apple.Safari = 1
```

## 詳しい説明（英語）

- [Switcher](docs/switcher.md): 3 つの表示、表示までの遅延、画面収録、パスワード管理アプリ。
- [Menu bar](docs/menubar.md): アイコン、`+k` のパネル、クリック。
- [Workspaces](docs/workspaces.md): 固定、どのデスクトップにも属さないアプリ、ウィンドウの呼び寄せ、枠、修飾キーの衝突。
- [App](docs/app.md): メニューバーのアプリ、メニュー、許可（準備中）。

## 設計

- **プロセスは 1 つ。** デスクトップ、スイッチャー、枠を、1 つの常駐プロセスで動かします。
- **ほぼ C。** Objective-C は `src/apps.m`（アプリの起動、終了、アクティブ化のイベント）と `src/menubar*.m`（メニューバーの項目とパネルとメニュー。`menubar` が `off` でないときだけ作る）です。`src/app.m` は、アプリ版の初回パネル、ログイン項目、設定の補助です。
- **枠は SkyLight で描きます。** JankyBorders と同じやり方で、NSWindow は作りません。
- **ホットキーは CGEventTap** で、メインのランループで受けます。スレッドは増やしません。

2 つのツールを組み合わせたものです。[AeroSpace](https://github.com/nikitabobko/AeroSpace)（使っていないデスクトップのウィンドウを画面の隅に退避する）と、[JankyBorders](https://github.com/FelixKratz/JankyBorders)（枠）。タイル配置、レイアウト、色を変えたいなら、そちらを使ってください。

## メモリ

`footprint` で測った値です。

| 状態 | メモリ |
|---|---|
| 待機中、`menubar = off` | 約 8MB |
| 待機中、`menubar = on` | 約 13.6MB |
| `list` を表示中 | 最大 約 16MB |
| `square` を表示中 | 最大 約 25MB |

スイッチャーのメモリは、`⌥` を離すと解放されます。詳しくは [switcher](docs/switcher.md#memory) と [menu bar](docs/menubar.md#memory) にあります。

## 開発

Xcode Command Line Tools と [mise](https://mise.jdx.dev) が必要です。`mise run build`、`mise run run`（ビルドしてフォアグラウンドで動かす）、`mise run test`、`mise run test-san`（同じテストを AddressSanitizer と UndefinedBehaviorSanitizer つきで動かす）を使います。`scripts/footprint.sh [config-file] [seconds]` は待機中のメモリをバイト数で出します。先に動いているものを止めてください。`v*` のタグを push すると、arm64 と x86_64 のバイナリが Releases に出ます。コードの地図は [docs/architecture.md](docs/architecture.md)（英語）です。モジュール、データの流れ、スレッド、メモリの決まりを書いています。

## 制限

- ウィンドウを退避するのは、メインのディスプレイだけです。複数のディスプレイには、まだ対応していません。
- SkyLight は Apple の非公開 API です。macOS の更新で、枠が動かなくなることがあります。
- クラッシュしたときは、退避したウィンドウが右下の隅に残ります。次に起動すると戻ります。
- 設定できるキーは、デスクトップのキーと基本の修飾キーだけです。

## ライセンス

[GPL-3.0](LICENSE)。

## 支援

just-desktops で毎日のキー操作が少しでも減ったら、[コーヒーをおごってもらえる](https://buymeacoffee.com/sijis)とうれしいです。
