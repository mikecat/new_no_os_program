# 10. ヒューリスティックに頼らないメモリマップの取得

これまで、ヒューリスティックを用いてメモリマップを取得していた。
しかし、これが本当に多くの環境で通用するかはわからない。
そのため、ヒューリスティックを使わないメモリマップの取得方法が求められる。
よく見ると、VRAMの情報の取得に用いた`EFI_BOOT_SERVICES`に、
`GetMemoryMap`というそれっぽいものがある。
そこで、これを活用する方法を開発した。

## GetMemoryMap

* [Decoding EFI memory map : osdev](https://www.reddit.com/r/osdev/comments/7sglch/decoding_efi_memory_map/)
* [iPXE: include/ipxe/efi/Uefi/UefiSpec.h File Reference](https://dox.ipxe.org/UefiSpec_8h.html#a6a58fcf17f205e9b4ff45fd9b198829a)

このへんを参考にすると、どうやら`GetMemoryMap`を用いると
メモリマップの情報を渡した領域に格納し、格納したサイズと1要素のサイズを教えてくれるようだ。
とりあえず取得してみよう。
ページングの初期化に使いたいので、まずはページングの初期化抜きで実行するようの
`10_memorymap.c`に実装した。

環境をあまり書き換えないため、x86版では素直に関数を呼べばいいが、
x64版では多くのことを考慮しないといけなそうだ。
Long Modeが有効になっているかをチェックし、有効ならx64版とみなした処理に入る。
この処理では、適切に引数を配置した後`GetMemoryMap`を呼び出し、
その後出力用にProtected Modeに切り替える。

ここで、拡張インラインアセンブリの`"m"`指定では、

* 配列を渡すと配列の先頭要素のアドレスではなく配列そのものが渡される。
* ならばと思って`&`を付けてアドレスにしようとすると、エラーになる。
* よって、ポインタを格納する変数を別に用意する。
* また、スタックポインタの位置を合わせないとうまくデータを読み書きできないようである。

という罠があった。
さらに、スタックポインタの位置はC言語の処理に戻った時の変数の操作にも影響を及ぼした。

気をつけて実装した結果、QEMU・VirtualBoxのx86・x64それぞれにおいて、
それっぽいメモリマップの情報を取得することができた。

## UEFIの関数を呼び出す方法の変更

VRAMの取得においては、以下のようにしてUEFIの関数を呼び出していた。

* x86の場合は、セグメント情報のみを起動時に戻し、呼び出す
* x64の場合は、
  1. インラインアセンブリを用い、スタックを下位に移す
  2. アセンブリ言語で書かれた関数を呼ぶ (ここで実行位置を下位に移す)
  3. この関数が以下の操作を行い、呼び出す
     * ページング解除
     * Long Modeへの移行
     * セグメント情報の復帰

これを、以下のように整理した。

* スタックの切り替えは、普通のC言語の関数`callWithIdentStack`を通して行う。
  (この関数は、アセンブリ言語で実装されている)
* UEFIの関数の呼び出しは、x86/x64共通で、以下のことを行う関数を呼ぶことで行う。
  * 実行位置を下位に移す
  * ページング解除
  * Long Modeへの移行 (x64の場合のみ)
  * セグメント情報の復帰

このことにより、UEFIの関数の呼び出しルーチンを再利用しやすくなった。

## ページングの初期化処理への組み込み

新しくしたUEFIの関数を呼ぶ関数を用い、ページングの初期化処理から`GetMemoryMap`を呼び出し、
その情報をこれまでのヒューリスティックにより得られたメモリマップの代わりに利用する。
この段階ではまだスタックを仮想メモリに移していないので、スタックの移動は不要である。
幸い、得られるメモリマップのフォーマットはx86とx64で共通のようなので、
x86とx64で分けるのは`GetMemoryMap`のアドレスを取得する部分のみですむ。
