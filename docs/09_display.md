# 9. ディスプレイを使いたい

## VGA? (いいえ)

シリアルポートもいいが、やはりディスプレイに情報を出力したい。
VGAを操作するというのが頭に浮かぶが、おまじないの塊である上に描画方法も独特で、かなり厄介だ。
第一、UEFIから起動された時はVGA非互換の設定になっており、
戻すにはエミュレータごとのおまじないを唱えることを要求される、といった感じだ。

参考:

* [VGA - os-wiki](http://oswiki.osask.jp/?VGA)
* [nop-act2/display.c at master · mikecat/nop-act2 · GitHub](https://github.com/mikecat/nop-act2/blob/master/src/display.c)
  * 先代のディスプレイ有効化コード。おまじないの塊。

## GOP?

どうやら、UEFIから起動される場合は、GOPとやらを使うのがいいらしい。
しかし、これを使うにはUEFIの関数を呼ぶことを要求されるようだ。
x64だとまた厄介そうなので、とりあえずx86から攻略していこう。

[GOP - OSDev Wiki](https://wiki.osdev.org/GOP)

この記事によると、どうやら`EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID`とやらを使うようだ。
ググると、こんなのが見つかった。

[Assembly - How to set graphics mode in UEFI (No VGA, No BIOS, Nothing deprecated) - Stack Overflow](https://stackoverflow.com/questions/52956762/assembly-how-to-set-graphics-mode-in-uefi-no-vga-no-bios-nothing-deprecated)

情報は不完全だが、具体的なデータ構造が示唆されており、かなり参考になりそうだ。
また、EFI Boot Services Tableとやらを要求されることがわかる。
とりあえずググった結果、これが出てきた。

[iPXE: EFI_BOOT_SERVICES Struct Reference](https://dox.ipxe.org/structEFI__BOOT__SERVICES.html)

これによれば、EFI Boot Services Tableはヘッダに続いて関数が並んでいる構造のようであり、
`LocateProtocol`は38番目(1-origin)の関数だ。

[iPXE: EFI_TABLE_HEADER Struct Reference](https://dox.ipxe.org/structEFI__TABLE__HEADER.html)
によればヘッダ24バイトであり、1個の関数が8バイトだとすれば、
`LocateProtocol`は0x140バイト目となり、先ほどのデータ構造と一致する。

## 要求されるもろもろを探す

さて、実際のメモリの中で見ていこう。
QEMUで`01_first.img`を実行し、gdbでメモリを観察する。
`EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID`の冒頭は`0x9042a9de`なので、これをfindすると、
2件見つかった。周りを見てみる。

```
(gdb) x/20wx 0x7e1cf14
0x7e1cf14:      0x9042a9de      0x4a3823dc      0xde7afb96      0x6a5180d0
0x7e1cf24:      0x09576e91      0x11d26d3f      0xa000398e      0x3b7269c9
0x7e1cf34:      0x7235c51c      0x4cab0c80      0x083bac87      0xb104634a
0x7e1cf44:      0x0f0b1735      0x419387a0      0x8c5366b2      0xce48af38
0x7e1cf54:      0xd9dcc5df      0x435e4007      0x70899890      0xb2045593
(gdb) x/20wx 0x7e5bdd8
0x7e5bdd8:      0x9042a9de      0x4a3823dc      0xde7afb96      0x6a5180d0
0x7e5bde8:      0x964e5b21      0x11d26459      0xa000398e      0x3b7269c9
0x7e5bdf8:      0x982c298b      0x41cbf4fa      0xaa7738b8      0x39b88f68
0x7e5be08:      0xdb9a1e3d      0x4abb45cb      0x38e53b85      0x2d2edb7f
0x7e5be18:      0x309de7f1      0x4ace7f5e      0x1b539cb4      0xef95aae5
```

うーん、よくわからないや。

やはりEFI Boot Services Tableの位置が欲しい。
もしかしたらACPIの情報を得た時の表にあるかもしれないと思い、
いくつかのGUIDが載っているサイトを見てみる。
[List EFI Configuration Table Entries « Musings](https://blog.fpmurphy.com/2015/10/list-efi-configuration-table-entries.html)

うーん…SERVICESって入っているから`EFI_DXE_SERVICES_TABLE_GUID`が近いかな？
先頭の`0x5ad34ba`を用いてテーブルを引く。
まず第2引数をチェックし、その周辺にテーブルがあると仮定して検索する。

```
(gdb) x/10wx $esp
0x7f90db0:      0x07f949b1      0x07568150      0x07f5f010      0x0756c5d0
0x7f90dc0:      0x0756c5d0      0x000000ed      0x00000000      0x00000000
0x7f90dd0:      0x00000000      0x00000000
(gdb) find /w 0x7f50000, 0x7f60000,0x5ad34ba
0x7f5f0a4
1 pattern found.
(gdb) x/5wx 0x7f5f0a4
0x7f5f0a4:      0x05ad34ba      0x42146f02      0xa04d2e95      0xb92b8e39
0x7f5f0b4:      0x07faad00
```

得られた場所の周辺を見てみる。

```
(gdb) x/100wx 0x07faad00
0x7faad00:      0x5f455844      0x56524553      0x00010028      0x00000060
0x7faad10:      0xf64c8d07      0x00000000      0x07f99751      0x07f995b5
0x7faad20:      0x07f98b32      0x07f98ba8      0x07f9826a      0x07f98c1e
0x7faad30:      0x07f98284      0x07f98dbf      0x07f99889      0x07f98e8c
0x7faad40:      0x07f98f02      0x07f99a25      0x07f99af0      0x07fa032a
0x7faad50:      0x07f9f90f      0x07f9fa0b      0x07f9e24b      0x07f98cd7
0x7faad60:      0x00000000      0x00000000      0x00000000      0x00000000
0x7faad70:      0x00000000      0x00000000      0x00000000      0x00000000
0x7faad80:      0x544f4f42      0x56524553      0x00020032      0x000000c8
0x7faad90:      0x4dc5a243      0x00000000      0x07f9ec3c      0x07f9ecb5
0x7faada0:      0x07f9c409      0x07f9c509      0x07f9b39b      0x07f9a9ca
0x7faadb0:      0x07f9ad96      0x07f9f619      0x07f9f08c      0x07f9f718
0x7faadc0:      0x07f9f637      0x07f9f784      0x07f9f69e      0x07f972e6
0x7faadd0:      0x07f9616d      0x07f96c6d      0x07f970a5      0x00000000
0x7faade0:      0x07f960d5      0x07f963c6      0x07f965da      0x07f95187
0x7faadf0:      0x07f94631      0x07f947e5      0x07f946dc      0x07f94ba5
0x7faae00:      0x07f920f4      0x07f1256e      0x07f950eb      0x07f95140
0x7faae10:      0x07f95778      0x07f95abd      0x07f96de5      0x07f97468
0x7faae20:      0x07f97563      0x07f97688      0x07f9682a      0x07f96781
0x7faae30:      0x07f97301      0x07f97407      0x07f31c0e      0x07fa276a
0x7faae40:      0x07fa2707      0x07f9f56f      0x00000000      0x07e68e38
0x7faae50:      0x07e5c2b8      0x07e9eed0      0x07edbe08      0x07ee5b44
0x7faae60:      0x07ef9580      0x07ef9584      0x07d8af50      0x040215d0
0x7faae70:      0x07fab498      0x49746547      0x206f666e      0x6d6f7266
0x7faae80:      0x69756720      0x20646564      0x74636573      0x206e6f69
```

EFI Boot Services Tableにしては短すぎると思われ、違いそうだ。
ただ、その下に長くて怪しいデータがある。
先頭部分を文字列と解釈すると、0x7faad00は`DXE_SERV(`、0x7faad80は`BOOTSERV2`である。
BOOTと入っているのでやはり下が怪しい。
この位置0x7faad80が書かれている場所を探した結果、多く見つかったが、
その中で第2引数に近いのは0x7f5f04cだった。
周辺を見てみる。

```
(gdb) x/100wx 0x7f5f010
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
0x7f5f110:      0xbbe52e99      0x94e320cf      0x07f3e000      0xeb9d2d30
0x7f5f120:      0x11d32d88      0x9000169a      0x4dc13f27      0x07f6b000
0x7f5f130:      0x8868e871      0x11d3e4f1      0x800022bc      0x81883cc7
0x7f5f140:      0x07f6b014      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f150:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f160:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f170:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f180:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x7f5f190:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
```

どうやら、第2引数が指す構造体の15番目(0-origin)のワード(4バイト)に入っているようだ。
表を検索する手間がかからず、むしろ好都合だ。

## 呼び出す方法を確認する

関数を呼ぶ時はUEFIのABIでないといけないようなので、呼び出し規約を確認する。

[UEFI - OSDev Wiki](https://wiki.osdev.org/UEFI#Calling_Conventions)

x86ではcdecl、x86-64ではMicrosoft's 64-bit calling conventionとのことだ。
具体的には？

[x86 calling conventions - Wikipedia](https://en.wikipedia.org/wiki/X86_calling_conventions)

cdeclとは、スタックに引数を積んで関数を呼ぶ「いつものやつ」のようだ。
すなわち、C言語から直接呼び出してよさそうなので助かる。
Microsoft's 64-bit calling conventionの情報もある。これは後で使うだろう。

GOPの操作では、`EFI_ERROR`と`EFI_NOT_STARTED`が出てくる。
Stack Overflowの記述より、`EFI_ERROR`は負かどうかを確認すればよいと推測できる。
`EFI_NOT_STARTED`は、ググれば出てきた。

[EFI_STATUSの値 | retrage.github.io](https://retrage.github.io/2019/11/26/efi-status-code.html)

64bit…？単純に0を減らせば正しい値になるかなあ？

## GOPのデータを得る

とりあえず、ここまでわかったことに基づき、GOPの位置を得てダンプするプログラム
`09_display_print_gop.c`を用意した。
`gop->data`のオフセットはStack Overflowによれば0x18のようだが、
x86とx64で違うかもしれない。

QEMUで実行すると、以下の出力が得られた。

```
gop = 76404A8
0x06d7a573 0x06d7a622 0x06d7a453 0x07640810 0x07640650
0x00000000 0x0000001e 0x0763fb10 0x0761d010 0x00000004
0x6c617470 0xafafafaf 0x0000005c 0xafafafaf 0xafafafaf
0xafafafaf 0xafafafaf 0xafafafaf 0xafafafaf 0xafafafaf
```

アドレスっぽい所のまわりを見てみる。

```
(gdb) x/20wx 0x7640810
0x7640810:      0x0000001e      0x00000002      0x076409d0      0x00000024
0x7640820:      0x80000000      0x00000000      0x001d4c00      0xafafafaf
0x7640830:      0x6c617470      0xafafafaf      0x0000003c      0xafafafaf
0x7640840:      0x30646870      0x00000000      0x00000004      0x00000040
0x7640850:      0x000c0102      0x0a0341d0      0x00000000      0x00060101
(gdb) x/20wx 0x7640650
0x7640650:      0x000c0102      0x0a0341d0      0x00000000      0x00060101
0x7640660:      0x03020200      0x01000008      0xff7f8001      0xafaf0004
0x7640670:      0x6c617470      0xafafafaf      0x0000003c      0xafafafaf
0x7640680:      0x30646870      0x00000001      0x00000004      0x00000054
0x7640690:      0x000c0102      0x0a0341d0      0x00000000      0x00060101
(gdb) x/20wx 0x763fb10
0x763fb10:      0x00000000      0x00000280      0x000001e0      0x00000020
0x763fb20:      0x0000003c      0x00000001      0x00000320      0x000001e0
0x763fb30:      0x00000020      0x0000003c      0x00000002      0x00000320
0x763fb40:      0x00000258      0x00000020      0x0000003c      0x00000003
0x763fb50:      0x00000340      0x00000270      0x00000020      0x0000003c
(gdb) x/20wx 0x761d010
0x761d010:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x761d020:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x761d030:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x761d040:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x761d050:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
```

Stack Overflowを参照すると、`gop->Mode`は
モード番号の最大、現在のモード番号、現在のモード情報、現在のモード情報のサイズ、
となっているはずである。
これによくあてはまるのは、0x7640810であり、そのオフセットは0xcだ。
よく見ると、さらにVRAMのアドレス、空、VRAMのサイズ、と続いている様子もうかがえる。

モード情報も見ておこう。

```
(gdb) x/20wx 0x076409d0
0x76409d0:      0x00000000      0x00000320      0x00000258      0x00000001
0x76409e0:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
0x76409f0:      0x00000320      0x6c617470      0xafafafaf      0x00000040
0x7640a00:      0x30646870      0x00000000      0x00000004      0x00000038
0x7640a10:      0x6c646f70      0x0763ff14      0x07672554      0x0768bd50
```

情報を統合すると、先頭からモードバージョン、横サイズ(800)、縦サイズ(600)、
ピクセルフォーマット(意味は謎)、謎×4、PixelsPerScanLine、という感じだろう。
計算すると、どうやらVRAMは1ピクセルあたり4バイトのようだ。

さらに、gopの最初の3ワード(4バイト)は、関数群と推測できる。
最初が`gop->QueryMode`、次が`gop->SetMode`と推測でき、その次はよくわからない。

これらの情報をもとに、`09_display_list.c`を作成した。
ページング有効版で起動すると、モードを変える所で固まってしまった。
おそらく、ページングによってVRAMまたはメモリマップドI/Oにアクセスできないのが問題なのだろう。
ページングを無効にすると、モードを変える部分も動いた。

## VRAMに書き込む

一旦モードを切り替えることは考えないことにして、得られたVRAMに書き込んでみる。
`09_display.c`である。
ページングを有効にするとVRAMへの書き込みが妨害されて失敗するので、無効にした。
結果、VRAMのピクセルは(PixelFormat=1の場合?)低位からB、G、R、?になっているようである。

## ライブラリに加える

ディスプレイの操作(VRAMの取得)をライブラリに加える。
そのために、まずはページングの操作のためのライブラリを整える。
具体的には、PDEにマッピングを書き込む関数を公開し、さらに領域を確保する関数を作成した。

次に、VRAMの情報を得る関数をライブラリに加える。
新しいファイル`display.c`を用いる。
このとき、得られたVRAMを仮想メモリにマップし、そのアドレスを保存する。

ついでに、既存のシリアルポートも含め、ライブラリ内各位の初期化をまとめて行う
`initialize.c`を追加した。

最後に、これを経由してきちんと画面表示ができることを確認するため、
`09_display_lib.c`を用意した。
