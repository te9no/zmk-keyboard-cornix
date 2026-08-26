# ZMK Keyboard for Cornix

CornixをZMKで使用するためのボード定義、Centralシールド、キーマップ、ビルド設定を収録したリポジトリです。

現在の標準構成は、ポインティングデバイスを搭載したCentral 1台と、Cornix左右のPeripheral 2台からなる3デバイス構成です。

```text
                     ┌─ cornix_ph_left（Peripheral）
Central（XIAO BLE）──┤
                     └─ cornix_right（Peripheral）
```

## Madula — 外部感覚器をつなぐ「人工髄」

Madulaは、Cornixに外部感覚器を接続するための「人工髄」です。

TrackPoint、Trackball、Analog Stickなど、性質の異なる入力器官を受け入れ、その操作信号をCornix本体とホストへ中継する中枢インターフェースとして設計されています。キーボードを単なるキー入力装置ではなく、用途に応じて感覚器を交換・拡張できる身体として扱うための接続点です。

現時点でファームウェアが対応しているMeKaBuモジュールは次の3種類です。

- PMW3610 Trackball
- ADS1220 LPPS TrackPoint
- Azoteq IQS9151 Trackpad

Analog StickなどはMadulaの拡張コンセプトに含まれますが、対応snippetは今後追加する必要があります。

## ビルドターゲット

実際の構成は[`build.yaml`](build.yaml)が正です。

### 通常使用

| ターゲット | 書き込み先 | 用途 |
| --- | --- | --- |
| `madula_trackball` | Madula Central | PMW3610 Trackball |
| `madula_trackpoint` | Madula Central | ADS1220 LPPS TrackPoint |
| `madula_iqs` | Madula Central | Azoteq IQS9151 Trackpad |
| `cornix_tps43_production` | Cornix TP Central | TPS43・DYA Studio・内蔵RGBステータスLED |
| `cornix_left_production` | Cornix左 | 外部Central構成用Peripheral |
| `cornix_right_production` | Cornix右 | 外部Central構成用Peripheral |

MadulaまたはTPS43のCentralファームウェアを1つ選び、左右のProductionファームウェアと組み合わせます。複数のCentralを同時には使用しません。

### ペアリング復旧

| ターゲット | 用途 |
| --- | --- |
| `cornix_tps43_host_bond_reset` | TPS43 Centralのホスト情報を消去し、左右Peripheralのbondは維持 |
| `cornix_left_bond_reset` | 左Peripheralの設定を一度だけ消去 |
| `cornix_right_bond_reset` | 右Peripheralの設定を一度だけ消去 |

### 全設定リセット

| ターゲット | 書き込み先 |
| --- | --- |
| `cornix_tps43_settings_reset` | XIAO BLE Central |
| `cornix_left_settings_reset` | Cornix左 |
| `cornix_right_settings_reset` | Cornix右 |

## `just.sh`でビルドする

このワークスペースのルートで実行します。初回のみ依存関係を初期化してください。

```bash
./just.sh init config/zmk-keyboard-cornix
```

### Madula + Trackball

```bash
./just.sh build madula_trackball
./just.sh build cornix_left_production
./just.sh build cornix_right_production
```

PMW3610の起動状態やレジスタをCDCへ定期出力する診断版が必要な場合だけ、
`madula-pmw-debug` snippetを追加します。通常のTrackballファームウェアでは
この周期ログは無効です。

```bash
./just.sh build madula_trackball -p always -S madula-pmw-debug
```

### Madula + TrackPoint

```bash
./just.sh build madula_trackpoint
./just.sh build cornix_left_production
./just.sh build cornix_right_production
```

### Madula + IQS Trackpad

```bash
./just.sh build madula_iqs
./just.sh build cornix_left_production
./just.sh build cornix_right_production
```

### Cornix TP + TPS43

```bash
./just.sh build cornix_tps43_production
./just.sh build cornix_left_production
./just.sh build cornix_right_production
```

全ターゲットをビルドする場合は次を使用します。

```bash
./just.sh build all
```

生成物は次のディレクトリへコピーされます。

```text
firmware/zmk-keyboard-cornix/<ブランチ名>/
```

## 書き込みと初回接続

1. 対象デバイスをUF2ブートローダーモードにします。通常はResetを素早く2回押します。
2. 表示されたUSBドライブへ、対応する`.uf2`ファイルをコピーします。
3. 左右PeripheralとCentralの電源を入れ直します。
4. ホストPCからCentralのBluetoothプロファイルへ接続します。

既存のbondが原因で接続できない場合は、上記のペアリング復旧ターゲットを使用してください。設定全体の消去は、ペアリング復旧で解決しない場合に限って使用します。

## Madula Centralのハードウェア

`madula_central`は、XIAO BLEを搭載したMadulaをCornixのSplit Centralとして動作させるシールドです。J4へ接続するMeKaBuモジュールに合わせ、Trackball、TrackPoint、IQS Trackpad用snippetのいずれかを選択します。

### J4モジュール端子

| J4 | 信号 | XIAO BLE GPIO | PMW3610 | ADS1220 LPPS | IQS9151 |
| --- | --- | --- | --- | --- | --- |
| 1 | NCS | P1.12 / D7 | CS | CS | IRQ |
| 2 | SCLK | P1.13 / D8 | SCLK | SCLK | I²C SCL |
| 3 | MOTION | P1.14 / D9 | Motion IRQ | MISO | 未使用 |
| 4 | SDIO | P1.15 / D10 | MOSI/MISO共用 | MOSI | I²C SDA |
| 5 | 3V3 | — | 3.3 V | 3.3 V | 3.3 V |
| 6 | GND | — | GND | GND | GND |

PMW3610では半二重SPIとしてP1.15をMOSI/MISOで共用します。ADS1220 LPPSではP1.14をMISO、P1.15をMOSIとして使用します。IQS9151は400 kHz I²C、アドレス`0x56`で動作し、P1.12をActive LowのIRQとして使用します。ジェスチャーとフィルターの初期値はGeaconPolarisの実装を基準にしています。

### Direct GPIOキー

SW3、SW4、SW5は、マトリクスを介さない独立したActive LowのDirect GPIOキーです。

| スイッチ | GPIO | キーマップ位置 | Baseレイヤー初期値 |
| --- | --- | --- | --- |
| SW3 | P0.04 / D4 | 50 | 左クリック |
| SW4 | P0.05 / D5 | 51 | 中クリック |
| SW5 | P1.11 / D6 | 52 | 右クリック |

### WS2812ステータスLED

SparAkashaAnantaの省電力実装を参考にしています。

- Data: P0.28 / D2
- LED電源制御: P0.03 / D1、Active Low
- LED数: 1
- バッテリー、接続、レイヤー表示でLED 0を共有
- 15秒後にLED電源を自動OFF
- バッテリー電圧: P0.02 / D0 / ADC0

## Cornix TP / TPS43 Central

`cornix_tps43_central`は、XIAO BLEとAzoteq TPS43をCornixのSplit Centralとして動作させます。TPS43は筐体内で90度回転しているため、ファームウェア側でX/Yの入れ替えと反転を行います。

### TPS43配線

以下は現在の[`cornix_tps43_central.overlay`](boards/shields/cornix_tps43_central/cornix_tps43_central.overlay)に基づく値です。

| TPS43 | XIAO BLE |
| --- | --- |
| VDDHI | 3V3 |
| GND | GND |
| SDA | P0.28 / D2 |
| SCL | P0.05 / D5 |
| RDY | P0.29 / D3 |
| NRST | P0.04 / D4 |
| Battery sense | P0.02 / D0 / ADC0 |

I2Cアドレスは`0x74`、バス速度はFast modeです。

### XIAO内蔵RGBステータスLED

起動時は、バッテリー状態を表示した後に接続状態を表示し、その後消灯します。

バッテリー表示：

| 色 | 状態 |
| --- | --- |
| 緑 | 80%以上 |
| 黄 | 20～79% |
| 赤 | 20%未満 |
| 白 | バッテリー値を取得できない |

接続表示：

| 色 | 状態 |
| --- | --- |
| シアン | USB接続 |
| 青 | BLE接続済み |
| 黄 | BLE広告中 |
| 赤 | 未接続 |

通常時は省電力のため消灯し、レイヤー変更時のみ120 ms点灯します。

| レイヤー | 色 |
| --- | --- |
| Base | 青 |
| Function | 赤 |
| Number | 緑 |
| Adjust | 黄 |
| Navigation | シアン |

## DYA Studio V2

`cornix_tps43_production`と`cornix_tps43_host_bond_reset`は、[DYA Studio](https://studio.dya.cormoran.works/)に対応しています。

TPS43 CentralをUSB接続し、ブラウザーからStudio用シリアルポートを選択してください。ZMKログ用ポートとは別のポートです。

主な機能：

- 物理レイアウト対応キーマップ編集
- Combo、Macroのランタイム設定
- 左右Encoderのレイヤー別設定
- US/JIS Layout Shift
- TPS43のカーソル、スクロール調整
- Bluetoothプロファイルと設定の管理
- デバイス、ファームウェア情報
- CentralのWatchdog、リセット履歴
- 左右Peripheralのキースイッチ診断
- スレッドスタック使用量などの開発情報

WatchdogはTPS43 Centralを監視します。KScan診断は左右Peripheralからの情報をCentral経由で集約します。Settings ResetファームウェアはStudioを公開しません。

## ボードとシールド

### ボード

| ID | 役割 |
| --- | --- |
| `cornix_left` | Cornix左を単独Centralとして使用する従来構成 |
| `cornix_ph_left` | 外部Central構成で使用する左Peripheral |
| `cornix_right` | Cornix右Peripheral |

Cornix PCBはEbyte E73-2G4M08S1C（nRF52840）を使用します。

### Centralシールド

| ID | 機能 |
| --- | --- |
| `madula_central` | MeKaBu Trackball／TrackPoint／IQS Trackpad対応Central |
| `cornix_tps43_central` | TPS43 Trackpad・DYA Studio対応Central |

## キーマップ

Madulaの入口キーマップは[`config/madula.keymap`](config/madula.keymap)です。共通の[`config/cornix.keymap`](config/cornix.keymap)を読み込み、3つのDirect GPIOキーを既存のCornix 50位置の後ろへ追加します。

キーマップ変更後は対象となるCentralと左右Peripheralを再ビルドしてください。

## SoftDeviceとブートローダー

Cornixの旧RMKファームウェアはSoftDeviceを使用しないFlash配置でした。本リポジトリの現行ボード定義もno-SD配置を使用するため、通常はSoftDeviceの復元なしで書き込めます。

古いファームウェアからの移行で起動できない場合は、[`bootloader/README.md`](bootloader/README.md)を確認してください。以前の設定データが残っている場合は、対応するSettings Resetファームウェアが必要になることがあります。

## リポジトリ構成

```text
boards/jzf/cornix/                 Cornix左右のボード定義
boards/shields/                    Madula・TPS43 Centralシールド
config/cornix.keymap               共通キーマップ
config/madula.keymap               Madula用キーマップ入口
config/west.yml                    ZMKと追加モジュールの依存関係
snippets/                          入力器官・復旧・LED設定
src/                               このリポジトリ固有のZMK拡張
build.yaml                         just.sh/GitHub Actionsのビルド対象
```

## ライセンス

各ファイルのSPDX表記に従います。
