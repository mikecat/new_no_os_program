# 11. UEFIによるテキスト出力

シリアルポートが無い実機で動作検証をする時、動作状況に関する情報が得られないと厳しい。
ACPIによる電源オフが動けば、そこに到達したという情報が得られるが、
もう少し複雑な情報が欲しくなる。

そういえば、UEFIにはテキストを出力するAPIがあったはずだ。
例えば、このへんのサイトで紹介されている。

* [UEFIベアメタルプログラミング - Hello UEFI!(ベアメタルプログラミングの流れについて) - へにゃぺんて＠日々勉強のまとめ](https://yohgami.hateblo.jp/entry/20170408/1491654807)
* [ツールキットを使わずに UEFI アプリケーションの Hello World! を作る - 品川高廣（東京大学）のブログ](https://shina-ecc.hatenadiary.org/entry/20140819/1408434995)

試してみよう。

## 渡される情報の観察

ACPIをいじったときと同様、最初に起動される時の第二引数
(記事によれば`EFI_SYSTEM_TABLE*`)が重要になりそうだ。
その定義は、ここでわかる。

[iPXE: EFI_SYSTEM_TABLE Struct Reference](https://dox.ipxe.org/structEFI__SYSTEM__TABLE.html)

今回は、特に`ConOut`というメンバが重要になりそうだ。
QEMUとgdbで観察し、実際のメモリ上の位置と対応付けよう。

x86では、以下のようになった。

```
(gdb) p/x $esp
$2 = 0x7f90db0
(gdb) x/8wx $esp
0x7f90db0:      0x07f949b1      0x07568150      0x07f5f010      0x0756c5d0
0x7f90dc0:      0x0756c5d0      0x000000ed      0x00000000      0x00000000
(gdb) x/64wx 0x07f5f010
0x7f5f010:      0x20494249      0x54535953      0x00020032      0x00000048
0x7f5f020:      0xe83f7919      0x00000000      0x07f3cf50      0x00010000
0x7f5f030:      0x07694dd0      0x07de46cc      0x07693f90      0x07de460c
0x7f5f040:      0x07693ed0      0x07de454c      0x07f5ff90      0x07faad80
0x7f5f050:      0x00000009      0x07f5f090      0x6c617470      0xafafafaf
0x7f5f060:      0x00000064      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f070:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f080:      0x30646870      0x00000008      0x00000006      0x0000015c
0x7f5f090:      0xee4e5898      0x42593914      0x7bdc6e9d      0xcf0394d7
0x7f5f0a0:      0x07d8bf10      0x05ad34ba      0x42146f02      0xa04d2e95
0x7f5f0b0:      0xb92b8e39      0x07faad00      0x7739f24c      0x11d493d7
0x7f5f0c0:      0x90003a9a      0x4dc13f27      0x07d88010      0x4c19049f
0x7f5f0d0:      0x4dd34137      0x978b109c      0xfafd3fa8      0x07fad140
0x7f5f0e0:      0x49152e77      0x47641ada      0xfe7aa2b7      0x8b5ed9fe
0x7f5f0f0:      0x07fab6d8      0xeb9d2d31      0x11d32d88      0x9000169a
0x7f5f100:      0x4dc13f27      0x07f40000      0xf2fd1544      0x4a2c9794
```

x64では、以下のようになった。

```
(gdb) p/x $rdx
$4 = 0x7eed018
(gdb) x/64wx 0x7eed018
0x7eed018:      0x20494249      0x54535953      0x00020032      0x00000078
0x7eed028:      0xf4a58d8b      0x00000000      0x06ddcfd8      0x00000000
0x7eed038:      0x00010000      0x00000000      0x0740bf18      0x00000000
0x7eed048:      0x06d53ad0      0x00000000      0x0740ae58      0x00000000
0x7eed058:      0x06d53c90      0x00000000      0x0740acd8      0x00000000
0x7eed068:      0x06d53dd0      0x00000000      0x07eede98      0x00000000
0x7eed078:      0x07f92900      0x00000000      0x00000009      0x00000000
0x7eed088:      0x07eed0d8      0x00000000      0x6c617470      0xafafafaf
0x7eed098:      0x000000a0      0x00000000      0xafafafaf      0xafafafaf
0x7eed0a8:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7eed0b8:      0xafafafaf      0xafafafaf      0x30646870      0x00000008
0x7eed0c8:      0x00000006      0x00000000      0x000001a8      0x00000000
0x7eed0d8:      0xee4e5898      0x42593914      0x7bdc6e9d      0xcf0394d7
0x7eed0e8:      0x07d19a58      0x00000000      0x05ad34ba      0x42146f02
0x7eed0f8:      0xa04d2e95      0xb92b8e39      0x07f92a80      0x00000000
0x7eed108:      0x7739f24c      0x11d493d7      0x90003a9a      0x4dc13f27
```

`ConOut`は、x86では11ワード目、x64では16ワード目であるようだ。 (0-origin)

## 実装

`11_conout.c`として、以下の実装を行った。

1. 第2引数のテーブルから`ConOut`を取得する
2. `ConOut`の指している場所から、`OutputString`を取得する
3. `OutputString`をUEFIの関数として呼び出す

結果、正常に文字列を出力することができた。
`OutputString`に渡す文字列は、1文字を2バイトで表現するのがコツのようである。

なお、`ConOut`が指す`EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`の構造は、ここが参考になる。
[iPXE: _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Struct Reference](https://dox.ipxe.org/struct__EFI__SIMPLE__TEXT__OUTPUT__PROTOCOL.html)

さらに、`OutputString`で使われている`EFI_TEXT_STRING`については、ここが参考になる。
[iPXE: include/ipxe/efi/Protocol/SimpleTextOut.h File Reference](https://dox.ipxe.org/SimpleTextOut_8h.html#afcf652d19afcb35e585089c15a51b115)

さらに、これを応用し、`lib/uefi_printf.c`にこれを用いて`printf()`風の出力をする関数を用意し、
`11_uefi_printf_test.c`でこの関数の動作確認を行った。
