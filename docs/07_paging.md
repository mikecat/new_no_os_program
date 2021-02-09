# 7. ページング

## 第一段階

4MBページングを用い、0xc0000000以降を0x00400000以降にマップする。
0xc0000000以降でプログラムを実行する。

4MBページングを有効にするには、cr4レジスタのpseビットを1にする。
[CPU Registers x86 - OSDev Wiki](https://wiki.osdev.org/CPU_Registers_x86)

PDE
[０から作るOS開発　ページングその１　ページとPTEとPDE](http://softwaretechnique.web.fc2.com/OS_Development/kernel_development07.html)

cr0レジスタのPGビットを1にすることで、ページングを有効にする。
[０から作るOS開発　カーネルローダその３　プロテクティッドモードへの移行とA20](http://softwaretechnique.web.fc2.com/OS_Development/kernel_loader3.html)

## 第2段階

例のヒューリスティックでメモリマップを得る。
available(7)のページを一覧にする。
普通に配列をグローバルで確保しようとすると起動失敗した。
でかすぎるようだ。
まず配列を確保できるだけの物理ページを集めて論理アドレスに配列を確保し、
その配列を用いて残りのリストアップを行う。
スタックも論理アドレスに確保し、切り替える。

## 第3段階

0xc0000000までは一様にマップしていたが、これをやめる。
PDE・PTE用の物理ページの確保とともに、そこにアクセスするための論理ページも確保しないといけない。
