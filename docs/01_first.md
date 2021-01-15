# 1. はじめの一歩

## 目的

まずは書いたプログラムをUEFIに起動してもらう方法を確立する。

## 用意するもの

* GCC (MinGW)
  * [MinGW - Minimalist GNU for Windows - Browse Files at SourceForge.net](https://sourceforge.net/projects/mingw/files/)
  * ファイルを拾い集める
* QEMU
  * [QEMU](https://www.qemu.org/)
  * Downloadから
* UEFI bios for QEMU
  * [GitHub - BlankOn/ovmf-blobs: BIOS.bin for qemu to support UEFI](https://github.com/BlankOn/ovmf-blobs)

## ビルドする

無限ループするだけの関数`entry`を含む`01_first.c`を用意した。

```
gcc -nostdlib -e _entry 01_first.c -o 01_first.exe
```

`-e`でエントリポイントを指定する。関数名は`entry`なのになぜか`_`を付けて指定すると吉。

UEFIで実行してもらうために、`01_first.exe`の中のヘッダの
`Opt.Subsys` ([TSXBIN](http://www.net3-tv.net/~m-tsuchy/tsuchy/dlpage.htm)での表示) を0x000Aに書き換える。

## バイナリをディスクイメージに入れる

適当なところから空のフロッピーイメージを持ってくる。

[DiskExplorer](https://hp.vector.co.jp/authors/VA013937/editdisk/index.html)でイメージを開く。
プロファイルの選択はplain imageにする。

ビルドして書き換えたexeファイルを`/EFI/BOOT/BOOTIA32.EFI`として保存する。
なぜこの名前か？知らない。過去の自分も出典を書いていなかった。
ルートディレクトリに`EFI`ディレクトリを作り、その中に`BOOT`ディレクトリを作り、
その`BOOT`ディレクトリの中に`BOOTIA32.EFI`を保存する、ということである。

## 起動する

QEMUをインストールし、バイナリにパスを通す。

UEFI bios for QEMUのファイルを用意する。 (zipで落として展開するなど)

```
qemu-system-i386 -bios bios32.bin -fda 01_first.img
```

`bios32.bin`は、UEFI bios for QEMUの中の`bios32.bin`のパスを指定する。

`first.img`は、バイナリを書き込んだフロッピーイメージのパスを指定する。

これで起動するが、イメージのフォーマット推測に関する警告が表示される。
これを避けるには、以下のおまじないでフロッピーイメージを指定する。

```
qemu-system-i386 -bios bios32.bin -drive format=raw,if=floppy,file=01_first.img
```

このおまじないの構築には、ここが参考になった。
[qemu-system-x86_64 -drive options - Heiko's Blog](https://heiko-sieger.info/qemu-system-x86_64-drive-options/)

うまく行けば、Boot Failed. EFI DVD/CDROMと出て止まるはずである。
何も出力せず無限ループするだけのプログラムを実行しているので、
何も起こらずに止まるのが成功である。

## 失敗

[Index of /repos](https://www.kraxel.org/repos/)で得られるOVMFや、
QEMUに最初から入っていたedk2を用いた起動を試みたが、

* (仮想)フロッピーディスクは、反応しない
* (仮想)ハードディスクは、メニューにハードディスクが出るが、ファイルは見られない

となり、失敗した。
今回のハードディスクイメージは、1024KiBの全部ゼロのファイルを作り、
partlogicでFATのパーティションを作ったものを用いた。

UEFI bios for QEMUでもハードディスクは認識させることができなかったが、
フロッピーディスクを認識させて起動することができたので、とりあえず今はヨシ…？
