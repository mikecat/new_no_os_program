# 12. 割り込み

## APIC

割り込みには、大きく分けて割り込みをする側と割り込みをされる側に分けられる。
まずは割り込みをする側を見ていこう。

割り込みをする側といえば、伝統的には2台の8259である。
これについては例えばここで解説されている。
[０から作るOS開発　割り込みその２　PICとIRQ](http://softwaretechnique.web.fc2.com/OS_Development/kernel_development03.html)
ただし、閲覧時点(2021/03/20)においてICW4の説明などに嘘があるので注意が必要である。

ただ、APICとやらを使うのがモダンらしい。
[Advanced Configuration and Power Interface Specification](https://www.intel.com/content/dam/www/public/us/en/documents/articles/acpi-config-power-interface-spec.pdf)
の5.2.11.4あたりを見ると、どうやらAPICにはLocal APICとI/O APICとやらがあるようだ。
Local APICについては、
[Intel® 64 and IA-32 Architectures Software Developer Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html)
の volume 3A: System programming guide, part 1 のChapter 10に載っている。
しかし、ここにはI/O APICの情報が無い。
ググった結果、I/O APICはチップセット側にあるらしい。

* [I/O APICについて - 睡分不足](https://mmi.hatenablog.com/entry/2017/04/09/132708)
* [IOAPIC datasheet for web - ioapic.pdf](https://pdos.csail.mit.edu/6.828/2017/readings/ia32/ioapic.pdf)

これらは後でゆっくり見るとして、まずはACPIのテーブルMADTを見ていこう。

## MADT

まずはQEMUのMADTをダンプしてみる。

```
(gdb) find /w 0x7e00000,0x7fffff0, 0x43495041
0x7e6b2e5
0x7f66000
0x7f66014
3 patterns found.
(gdb) x/32wx 0x7f66014
0x7f66014:      0x43495041      0x00000001      0x43505842      0x00000001
0x7f66024:      0xfee00000      0x00000001      0x00000800      0x00000001
0x7f66034:      0x00000c01      0xfec00000      0x00000000      0x00000a02
0x7f66044:      0x00000002      0x0a020000      0x00050500      0x000d0000
0x7f66054:      0x09000a02      0x00000009      0x0a02000d      0x000a0a00
0x7f66064:      0x000d0000      0x0b000a02      0x0000000b      0x0604000d
0x7f66074:      0x010000ff      0xafafafaf      0xafafafaf      0xafafafaf
0x7f66084:      0xafafafaf      0xafafafaf      0xafafafaf      0xafafafaf
(gdb) x/32wx 0x7f66000
0x7f66000:      0x43495041      0x00000078      0x4f42ed01      0x20534843
0x7f66010:      0x43505842      0x43495041      0x00000001      0x43505842
0x7f66020:      0x00000001      0xfee00000      0x00000001      0x00000800
0x7f66030:      0x00000001      0x00000c01      0xfec00000      0x00000000
0x7f66040:      0x00000a02      0x00000002      0x0a020000      0x00050500
0x7f66050:      0x000d0000      0x09000a02      0x00000009      0x0a02000d
0x7f66060:      0x000a0a00      0x000d0000      0x0b000a02      0x0000000b
0x7f66070:      0x0604000d      0x010000ff      0xafafafaf      0xafafafaf
```

Local APICやI/O APICのアドレスなどの情報が入っているようだ。
これらをよりあえず読んでみるプログラム`12_madt.c`を作った。

QEMUでの実行結果は、以下のようになった。

```
Local APIC Address = 0xFEE00000
Flags = 0x00000001
Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x00
APIC ID = 0x00
Flags = 0x00000001

Type = 1
Length = 12
I/O APIC structure
I/O APIC ID = 0x00
I/O APIC Address = 0xFEC00000
Global System Interrupt Base = 0x00000000

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 0
Global System Interrupt = 0x00000002
Flags = 0x0000

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 5
Global System Interrupt = 0x00000005
Flags = 0x000D

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 9
Global System Interrupt = 0x00000009
Flags = 0x000D

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 10
Global System Interrupt = 0x0000000A
Flags = 0x000D

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 11
Global System Interrupt = 0x0000000B
Flags = 0x000D

Type = 4
Length = 6
Local APIC NMI Structure
ACPI Processor ID = 0xFF
Flags = 0x0000
Local APIC LINT# = 1
```

VirtualBoxでの実行結果は、以下のようになった。
ただし、「I/O APICを有効化」にチェックを入れないとMADT not foundとなった。

```
Local APIC Address = 0xFEE00000
Flags = 0x00000001
Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 0
Global System Interrupt = 0x00000002
Flags = 0x0000

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 9
Global System Interrupt = 0x00000009
Flags = 0x000F

Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x00
APIC ID = 0x00
Flags = 0x00000001

Type = 1
Length = 12
I/O APIC structure
I/O APIC ID = 0x01
I/O APIC Address = 0xFEC00000
Global System Interrupt Base = 0x00000000
```

LattePandaでの実行結果は、以下のようになった。

```
Local APIC Address = 0xFEE00000
Flags = 0x00000001
Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x01
APIC ID = 0x00
Flags = 0x00000001

Type = 4
Length = 6
Local APIC NMI Structure
ACPI Processor ID = 0x01
Flags = 0x0005
Local APIC LINT# = 1

Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x02
APIC ID = 0x02
Flags = 0x00000001

Type = 4
Length = 6
Local APIC NMI Structure
ACPI Processor ID = 0x02
Flags = 0x0005
Local APIC LINT# = 1

Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x03
APIC ID = 0x04
Flags = 0x00000001

Type = 4
Length = 6
Local APIC NMI Structure
ACPI Processor ID = 0x03
Flags = 0x0005
Local APIC LINT# = 1

Type = 0
Length = 8
Processor Local APIC structure
ACPI Processor ID = 0x04
APIC ID = 0x06
Flags = 0x00000001

Type = 4
Length = 6
Local APIC NMI Structure
ACPI Processor ID = 0x04
Flags = 0x0005
Local APIC LINT# = 1

Type = 1
Length = 12
I/O APIC structure
I/O APIC ID = 0x01
I/O APIC Address = 0xFEC00000
Global System Interrupt Base = 0x00000000

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 0
Global System Interrupt = 0x00000002
Flags = 0x0000

Type = 2
Length = 10
Interrupt Source Override
Bus = 0
Source = 9
Global System Interrupt = 0x00000009
Flags = 0x000D
```

## IDT

割り込みをする側は難しそうなので一旦おいといて、割り込みをされる側を見ていく。
割り込みをされるには、IDTに割り込みハンドラの情報を登録しないといけない。
これには、以下のサイトが参考になる。

* [０から作るOS開発　割り込みその１　割り込みとIDTとGDT](http://softwaretechnique.web.fc2.com/OS_Development/kernel_development02.html)
* [Basic x86 interrupts | There is no magic here](https://alex.dzyoba.com/blog/os-interrupts/)
* [Exceptions - OSDev Wiki](https://wiki.osdev.org/Exceptions)

これを参考に、以下の動作をする割り込みハンドラをアセンブリ言語で作成する。
割り込み番号ごとにハンドラを作るのは手間なので、Perlのプログラムでアセンブリ言語のソースを生成する。
このテクニックは、[xv6](https://github.com/mit-pdos/xv6-public)でも使われている。

1, エラーコードが積まれない例外の場合、ダミーのエラーコードをpushする
2. 割り込み番号をpushする
3. 整数レジスタの値を保存する
4. セグメントレジスタの値を保存し、セグメントを切り替える
5. C言語の割り込みハンドラを呼ぶ
6. セグメントを戻す
7. 整数レジスタの値を戻す
8. 割り込み番号とエラーコードをスタックから取り除く
9. iret

これを実装することで、ゼロ除算などの例外をキャッチすることができるようになる。
これを実験するのが、`12_divzero.c`である。

## 8259

まずは伝統的な8259の操作を行う。
これは、以下からなる。

* ICW1～ICW4を書き込む
* マスクを書き込む
* 割り込みが来たら、EOIを書き込む

これらを実装し、テストとしてタイマ(8253)を扱う`12_timer_device_test.c`を用意した。

## xAPIC / x2APIC

以下の手順を用い、xAPIC / x2APICを有効化する。

1. CPUIDでAPICの存在をチェックする
2. MADTの存在をチェックする
3. MADTからLocal APICとI/O APICの情報と、IRQの割り当ての情報を取得する
4. I/O APICを仮想メモリに割り当て、要素数の情報を取得する
5. IRQをI/O APICに割り当てる
6. CPUIDでx2APICの存在をチェックし、x2APICまたはxAPICを初期化する
7. I/O APICを初期化する
8. MADTとAPIC IDからprocessor IDを得る
9. 得られたprocessor IDを用いて、MADTに基づいてNMIの設定を行う
10. IRQ用にI/O APICを設定する

また、マスクやEOIの処理もxAPIC / x2APIC用のものを追加する。

なお、LVT CMCI Registerについてはoptionalのようであり、
実際にQEMUでは存在しないとしてエラーになっていたようなので、
CPUIDでMachine Checkの有無を判定し、ある場合のみ初期化するようにした。

参考：[assembly - Questions about APIC interrupt - Stack Overflow](https://stackoverflow.com/questions/57373473/questions-about-apic-interrupt)

…が、`qemu-system-x86_64`ではこのチェックをすり抜けてLVT CMDI Registerにアクセスし、エラーになった。
そこで、LVT CMCI Registerの初期化はとりあえず完全に無くすことにした。
