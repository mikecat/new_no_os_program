# 2. 単純な実行用ツール

前回、UEFIから自分のプログラムを実行してもらう方法を構築することができた。
しかし、いちいちバイナリエディタでexeファイルを書き換え、
DiskExplorerでディスクイメージに書き込む、というのはめんどくさい。
そこで、この作業をかわりにやってもらうプログラムを作ることにした。

このプログラムが、`exe2img.pl`である。
入力のexeファイルと出力のフロッピーイメージファイルを指定すると、
exeファイルの`Opt.Subsys`を0x000Aに書き換えたデータが
ファイル`/EFI/BOOT/BOOTIA32.EFI`として保存されたフロッピーイメージファイルを作ってくれる。

本当はディスクイメージ内の他のファイルを保持する
「ファイルをイメージファイルに書き込む」ツールの方がいいかもしれないが、
ディスクの読み書きができるようになるのは当分先かもしれないし、今はこれでいいだろう。

ディスクイメージの作成には、既存のディスクイメージおよび

* [FATファイルシステムのしくみと操作法](http://elm-chan.org/docs/fat.html)
* [(AT)BIOS - os-wiki](http://oswiki.osask.jp/?%28AT%29BIOS)

を参考にした。
最初のヘッダのプログラムのソースコードは、`disk_header.asm`である。
