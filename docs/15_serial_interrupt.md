# 15. 割り込みを用いたシリアルポート操作

* 送信・受信それぞれリングバッファを用意する
* FIFOを用い、ある程度まとめて送受信できるようにする
* 変な所で割り込みが入って災いが起きないよう、とりあえずcli・stiで保護する
  * とりあえず安全側に倒しておきたい、パフォーマンスは余裕があれば
  * 割り込み無効状態で来たかもしれないので、stiは無条件にはしない
  * 将来マルチコア対応にする場合、cli・stiではなくロックが要る？
* 割り込み無効状態でブロッキングする関数に入って詰まないよう、送受信の関数を呼ぶ
* 初期化前は直接操作にフォールバックする

`14_text_display_test.c`の入力を割り込みを用いるものに置き換えた
`15_text_display_test.c`を用意した。
バッファのおかげか、約4096バイトまでの入力は連続(Tera Termで遅延0msで貼り付け)で与えても
きちんと読み込めたが、それを超えるとダメだった。

また、大量の出力を行う`15_print_many.c`も用意した。
出力はバッファが空くまで待つ仕様なので、バッファ容量を超えてもきちんと出力できた。
