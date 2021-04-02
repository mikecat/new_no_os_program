# 13. SIMDを用いたデータコピー

## 画面表示の高速化

`09_display_lib.c`は色を計算しながら1バイトずつ書き込むプログラムだったが、
これは環境によっては描画の過程がはっきり見えるほど遅かった。
もっと高速に書き込めるようにしないと、実用は難しそうだ。

そこで、まず事前に色を計算し、それを4バイトずつコピーするプログラム
`13_display_lib_buffer.c`を作成した。
これにより1バイトずつ書き込むよりは速くなったが、まだ描画の過程が見える。

## SIMDを用いたデータコピー

4バイトずつの書き込みでも遅かったので、
、SIMD(AVX/SSE)を使用してさらなる高速化を図ったプログラム
`13_display_lib_simd.c`を作成した。
その結果、描画の過程があまり目立たないようになった。

なお、SIMDを使用するには、有効化フラグを立てないといけないようである。
その前に、CPUが対応しているかチェックするべきだ。
対応しているかのチェックは、CPUID命令を用いて、
SSEはFXSRとSSEに対応しているか、AVXはXSAVEとAVXに対応しているかで判定できる。

対応チェックについては、以下のサイトなどが参考になる。

* [CPUID - OSDev Wiki](https://wiki.osdev.org/CPUID)
* [waura's blog – CPUID命令を使用して、SSEに対応しているか調べるコード](https://waura.github.io/200909121252748821.html)
* [Is AVX enabled?](https://software.intel.com/content/www/us/en/develop/blogs/is-avx-enabled.html)
  * この記事ではXSAVEの存在をビット27で判定しているが、このビットはCR4の設定に連動するため、ビット26で判定するべきである。

SSEを有効化するには、CR4の9ビット目(OSFXSR)を1にする。
AVXを有効化するには、CR4の18ビット目(OSXSAVE)を1にし、
さらにXCR0の1ビット目(SSE)と2ビット目(AVX)を1にする。

有効化については、以下のサイトなどが参考になる。

* [CPU Registers x86-64 - OSDev Wiki](https://wiki.osdev.org/CPU_Registers_x86-64)
* [XGETBV — Get Value of Extended Control Register](https://www.felixcloutier.com/x86/xgetbv)
* [XSETBV — Set Extended Control Register](https://www.felixcloutier.com/x86/xsetbv)
* [Control register - Wikipedia](https://en.wikipedia.org/wiki/Control_register)

今回はデータのコピーをするだけなので、例外の処理や割り込み時のレジスタの退避は考えなくて良いが、
SIMDを本格的に利用したい場合は、それらを実装することになるだろう。
それはまた要求される時に考えることとする。
