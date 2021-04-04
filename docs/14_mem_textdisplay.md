# 14. メモリ操作とテキストの画面表示

## メモリ操作

メモリを扱う汎用的な関数を、`lib/memory_utils.c`として実装する。

### SIMDが使えるかのチェック

前回「13. SIMDを用いたデータコピー」でもSIMD(SSE・AVX)が使えるかの判定を実装したが、
今回は整理して関数化した。
ついでに、使うかはわからないが、他の種類(SSE2～SSE4.2、AVX2、FMA)についても判定を実装した。

このあたりが参考になる。

* [CPUID - Wikipedia](https://en.wikipedia.org/wiki/CPUID)
* [sandpile.org -- x86 architecture -- CPUID](https://sandpile.org/x86/cpuid.htm)
* [Intel® Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#)
  * SIMD命令の種類がわかりやすい。
* [Check the CPU instruction set support level (SSE, AVX, AVX2, F16C, FMA, FMA4, XOP)(Others-Community)](https://titanwolf.org/Network/Articles/Article?AID=759c3624-12db-4bce-8113-875bd2e8a7cd#gsc.tab=0)
  * FMAの判定の前にAVXの判定をするべきことなどがわかる。

### メモリのコピー

メモリをコピーする際は、コピーによってコピー元が無駄に破壊されないように気をつけるべきである。
「無駄でない破壊」は、コピー元とコピー先が重なっている場合における、
コピー元のうちコピー先に含まれる部分の破壊である。
「無駄な破壊」は、それ以外の破壊である。
特に、コピー先にもなっているコピー元を、コピーする前に破壊してはいけない。

このためには、コピー元がコピー先より前にある場合は後ろからコピーを、
コピー元がコピー先より後ろにある場合は前からコピーをするとよい。
なお、コピー元とコピー先が同じ場合は、そもそもコピーは不要である。

さらに、SIMDが使える場合はSIMDを使うようにした。
具体的には、

1. 最初の半端な部分をコピーし、コピー元のアラインメントを合わせる
2. ブロック単位でのコピーを行う
3. 最後の半端な部分をコピーする

という処理をするようにした。
ただし、コピー元のアラインメントを合わせても、
コピー先のアラインメントも合っているとは限らないことに注意が必要である。

[what are the performance implications of using vmovups and vmovapd instructions respectively? - Intel Community](https://community.intel.com/t5/Intel-ISA-Extensions/what-are-the-performance-implications-of-using-vmovups-and/td-p/1143448)

によれば、十分新しいCPUならアラインメントを要求しない命令でも
アラインメントされていればパフォーマンスが落ちないらしいので、
場合分けはせずにコピー先にはアラインメントを要求しない命令を使っておけばいいだろう。

インラインアセンブラの破壊するレジスタに`%xmm0`や`%ymm0`を指定したところ、
error: unknown register name となってしまったので、指定を外した。
これらのレジスタはコピーで使うためデータは破壊されるが、いいのかなあ…？

## テキストの画面表示

シリアルポートは実機だとついていないことがあるし、
UEFIの文字列出力にもいつまでも頼っていられないかもしれない。
そこで、ディスプレイに文字列を表示する仕組みがあると便利だろう。

### フォント

文字列を表示するには、フォントを用意するのが一般的だろう。
フォントとしては「どのような線を描くか」のベクトルデータを用意する方法もあるが、
今回は簡単な文字の画像を用意する方法を使う。

具体的には、まず一般的なお絵かきソフトで扱える画像ファイル(24ビットBMP)を用意し、
それを`lib/font_gen.pl`でC言語のソースファイルに変換することで、フォントデータを用意することとした。

今回は1バイトの文字だけを対象とする。
文字コードとフォントの対応関係は、ここが参考になる。

* [ASCIIコード表](https://www.k-cube.co.jp/wakaba/server/ascii_code.html)
* [Shift_JIS 文字コード表](https://seiai.ed.jp/sys/text/java/shiftjis_table.html)

### 画面データ

「9. ディスプレイを使いたい」ではデータと色の関係を決め打ちしたが、
もう少しちゃんと調べてみる。

* [iPXE: EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Struct Reference](https://dox.ipxe.org/structEFI__GRAPHICS__OUTPUT__MODE__INFORMATION.html)
* [iPXE: EFI_PIXEL_BITMASK Struct Reference](https://dox.ipxe.org/structEFI__PIXEL__BITMASK.html)
* [iPXE: include/ipxe/efi/Protocol/GraphicsOutput.h File Reference](https://dox.ipxe.org/GraphicsOutput_8h.html#a2c8fb8916927427fbdeeff36dbecefca)
* [UEFI BIOSではGOP driverが必須に！ | DXR165の備忘録](https://dxr165.blog.fc2.com/blog-entry-451.html)

「9. ディスプレイを使いたい」のモード情報で「謎×4」だった部分は、
ピクセルのビットマスク情報が入ることがあるようだ。
具体的な意味はよくわからないが、多分1になっている所の下位から順に、
輝度の下位から上位に向かってビットを入れていく、みたいな感じだろう。 (推測)
また、1ピクセルあたり32ビット(4バイト)で固定のようである。

ピウセルフォーマットをもとに色(R・G・B)から色データ(32ビット)を作り、
整数として返す関数を使って使うのがいいだろう。
