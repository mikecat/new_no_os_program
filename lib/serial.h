#ifndef SERIAL_H_GUARD_1D632BA7_ACF5_4754_8512_AEED31C9C1D8
#define SERIAL_H_GUARD_1D632BA7_ACF5_4754_8512_AEED31C9C1D8

#include <stdarg.h>

int initSerial(void);

void flushSerial(void);

/* 実際に書き込んだバイト数を返す */
int serialSendData(const void* data, int dataSize);

/* 実際に読みだしたバイト数を返す。入力は待たず、今あるデータを返す */
int serialReceiveData(void* data, int dataSize);

/* バッファの空きバイト数を返す */
int serialGetTxBufferAvailable(void);

/* バッファにあるデータのバイト数を返す */
int serialGetRxBufferAvailable(void);

/* dataSizeバイト書き込んでから帰る */
void serialSendDataBlocking(const void* data, int dataSize);

/* dataSizeバイト読み込んでから帰る */
void serialReceiveDataBlocking(void* data, int dataSize);

/* 1文字書き込む */
int serialPutchar(int c);

/* 1文字読み込む。入力があるまで待つ */
int serialGetchar(void);

/* 1文字読み込む。データが無い時は-1を返す */
int serialGetcharNowait(void);

int vprintfSerial(const char* format, va_list args);
int printfSerial(const char* format, ...);

#endif
