# 16. タイマー

## 8253

8253は、昔ながらのタイマーデバイスである。
I/Oポートを用いて操作を行う。
タイムアウト時、IRQ0を発生させる。

詳しくは → [０から作るOS開発　割り込みその３　PICのまとめとPITと割り込みハンドラ](http://softwaretechnique.web.fc2.com/OS_Development/kernel_development04.html)

## APIC Timer

APIC Timerは、Local APICの機能の1つである。
大まかな利用方法は、

1. LVT Timer Register で割り込みのベクタ番号とモードを設定する
2. Divide Configuration Register (for Timer) で速度(分周の比)を設定する
3. Initial Count Register (for Timer) で時間を設定する

となる。
速度は決まっていないようなので、速度が決まっているACPIのPower Management Timerで1秒を測り、
その間にAPIC Timerのカウントがどれだけ進んだかを調べる。
そして、それに基づいて時間を設定する。

## リンクリストを用いたタイムアウト処理

タイムアウト時に実行する処理を、リンクリストを用いてタイムアウトが近い順に管理する。
こうしておくことで、タイマー割り込み時に先頭のノードの残り時間を減らすだけで更新できる。

1回に減らす残り時間(=タイマー割り込みの間隔)は最初1msとしたが、
QEMUで設定の約2倍の時間がかかる現象が見られたため、4msとした。
