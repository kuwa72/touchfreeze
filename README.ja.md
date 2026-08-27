# TouchFreeze

[English version / 英語版](./README.md)

TouchFreeze は、キーボード入力中に誤ってタッチパッドやタッチスクリーンが反応するのを防ぐ、Windows 用の軽量ユーティリティです。システムトレイに常駐し、タッチパッドを完全に無効化せず、必要なときだけ一時的に入力を抑制します。

## なぜ使うのか

- 手のひらがタッチパッドに触れても、誤クリック・カーソルジャンプを減らせます。
- タッチパッドを完全に切らず、入力直後だけ抑制します。
- 通常のタッチパッドと Windows Precision Touchpad (PTP) の両方に対応。
- 右ドラッグゾーンを使えば、指定したエリアを右クリック＋ドラッグとして使えます。
- CPU・メモリ消費が小さく、システムトレイで静かに動作します。

## スクリーンショット

### システムトレイメニュー
![システムトレイのコンテキストメニュー](docs/screenshots/tray-menu.png)

トレイアイコンを右クリックすると、自動起動、ブロック時間、設定、カーソル固定などを選択できます。

### 入力モニター
![入力モニター](docs/screenshots/input-monitor.png)

内蔵モニターで、パームリジェクションの状態、タッチ座標、イベントストリームをリアルタイムに確認できます。自分の端末に合わせた調整が可能です。

## インストール

1. [Releases](https://github.com/kuwa72/touchfreeze/releases) から `TouchFreeze.msi` をダウンロードします。
2. インストーラーを実行します。
3. スタートメニューから TouchFreeze を起動します。システムトレイにアイコンが表示されます。

最新の CI ビルドを手動で取得する場合は:

```sh
./scripts/fetch-latest-build.sh
```

## 使い方

TouchFreeze を起動すると自動的に動作します。

トレイアイコンを右クリックして設定を変更できます:

- **Auto Start**: Windows 起動時に TouchFreeze を自動起動する。
- **Block Time**: キー入力後、入力を抑制する時間（300 ms / 500 ms / 700 ms）。
- **Freeze Cursor While Typing**: ブロック時間中、カーソル移動も抑制する。
- **Touchpad Settings / Monitor...**: 入力モニターを開き、右ドラッグゾーンを設定する。
- **Right Drag Zone**: ワンフィンガー右ドラッグに使うタッチパッド領域を選択する。

## 動作の概要

TouchFreeze は低レベルのキーボードフックとマウスフックを利用します。キーを押すと短い抑制ウィンドウが開始され、その間は物理クリックや（オプションで）カーソル移動をブロックします。ウィンドウが終わると入力は再び許可されます。

タッチパッドでは、Raw HID 入力からマルチタッチ、confidence、移動距離を読み取り、手のひらによるタッチと意図的なカーソル操作を区別します。タイピングウィンドウが終わった後も、クールダウン期間中はパームタッチを抑制できます。

詳細は [ARCHITECTURE.md](ARCHITECTURE.md) を参照してください。

## 動作環境

- Windows 10 / Windows 11
- 通常のタッチパッド、または Windows Precision Touchpad (PTP)

## 開発

- **Solution**: `TouchFreeze.sln`
- **Toolset**: Visual Studio 2022, MSBuild v143
- **Build**: `powershell -ExecutionPolicy Bypass -File .\build.ps1`
- **Release**: WSL なら `bash scripts/release.sh`、Windows なら `powershell -ExecutionPolicy Bypass -File .\release.ps1`

## ライセンス

[License.txt](License.txt) を参照してください。
