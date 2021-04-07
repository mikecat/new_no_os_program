#include "serial.h"
#include "io_macro.h"
#include "interrupt.h"
#include "pages.h"
#include "serial_direct.h"
#include "my_printf.h"

#define TX_BUFFER_SIZE 4096
#define RX_BUFFER_SIZE 4096

static int initialized = 0;

static int fifo_available;

static int tx_buffer_start, tx_buffer_end;
static unsigned char* tx_buffer;

static int rx_buffer_start, rx_buffer_end;
static unsigned char* rx_buffer;

static void transmitData() {
	int status;
	int iflag = getIF();
	cli();
	in8(status, 0x03FD);
	/* TX FIFO empty */
	if (status & 0x20) {
		int i;
		/* send at most 16 bytes if FIFO is available */
		for (i = (fifo_available ? 0 : 15); i < 16; i++) {
			/* break when the TX buffer is empty */
			if (tx_buffer_start == tx_buffer_end) break;
			/* send one byte */
			out8(tx_buffer[tx_buffer_start++], 0x03F8);
			if (tx_buffer_start >= TX_BUFFER_SIZE) tx_buffer_start = 0;
		}
	}
	if (tx_buffer_start != tx_buffer_end) {
		/* more data to send available : enable TX available interrupt */
		int ien;
		in8(ien, 0x03F9);
		out8(ien | 2, 0x03F9);
	} else {
		/* no data to send : disable TX available interrupt */
		int ien;
		in8(ien, 0x03F9);
		out8(ien & ~2, 0x03F9);
	}
	if (iflag) sti();
}

static void receiveData(void) {
	int iflag = getIF();
	cli();
	for (;;) {
		int rxFlag, data;
		/* check if more data is available */
		in8(rxFlag, 0x03FD);
		if (!(rxFlag & 1)) break;
		/* read the data */
		in8(data, 0x03F8);
		/* store the data to buffer if it is not full */
		if ((rx_buffer_end + 1) % RX_BUFFER_SIZE != rx_buffer_start) {
			rx_buffer[rx_buffer_end++] = (unsigned char)data;
			if (rx_buffer_end >= RX_BUFFER_SIZE) rx_buffer_end = 0;
		}
	}
	if (iflag) sti();
}

static void serialInterruptHandler(struct interrupt_regs* regs) {
	int interruptId;
	(void)regs;
	in8(interruptId, 0x03FA);
	if (interruptId & 1) return; /* no interrupt */
	switch ((interruptId >> 1) & 7) {
		case 0: /* modem status */
			{
				int status;
				in8(status, 0x03FE);
			}
			break;
		case 3: /* line status */
			{
				int status;
				in8(status, 0x03FD);
			}
			break;
		case 1: /* TX available */
			transmitData();
			break;
		case 2: /* RX available */
		case 6: /* RX available (timeout) */
			receiveData();
			break;
	}
}

int initSerial(void) {
	unsigned int* cr3;
	int interruptId;
	/* initialize buffer */
	get_cr3(cr3);
	tx_buffer = (unsigned char*)allocate_region(cr3, TX_BUFFER_SIZE);
	rx_buffer = (unsigned char*)allocate_region(cr3, RX_BUFFER_SIZE);
	tx_buffer_start = tx_buffer_end = 0;
	rx_buffer_start = rx_buffer_end = 0;

	/* enable FIFO, set RX interrupt threshold = 8 bytes */
	out8(0x81, 0x03FA);
	/* enable RX interrupt */
	out8(0x01, 0x03F9);
	/* check if FIFO is available */
	in8(interruptId, 0x03FA);
	fifo_available = !!(interruptId & 0xC0);
	/* register interrupt */
	registerInterruptHandler(0x24, serialInterruptHandler);
	/* enable interrupt */
	setInterruptMask(4, 0);

	/* mark as initialized */
	initialized = 1;
	return 1;
}

void flushSerial(void) {
	int ien;
	int iflag = getIF();
	if (!initialized) return;
	cli();
	/* disable TX available interrupt */
	in8(ien, 0x03F9);
	out8(ien & ~2, 0x03F9);
	/* flush */
	while (tx_buffer_start != tx_buffer_end) {
		putchar_serial_direct(tx_buffer[tx_buffer_start]);
		tx_buffer_start++;
		if (tx_buffer_start >= TX_BUFFER_SIZE) tx_buffer_start = 0;
		if (iflag) {
			sti();
			cli();
		}
	}
	if (iflag) sti();
}

int serialSendData(const void* data, int dataSize) {
	const unsigned char* udata = data;
	int i;
	int sentSize = 0;
	int iflag = getIF();
	if (dataSize <= 0) return 0;
	if (!initialized) {
		for (i = 0; i < dataSize; i++) putchar_serial_direct(udata[i]);
		return dataSize;
	}
	cli();
	if (tx_buffer_start <= tx_buffer_end) {
		int maxCopySize = TX_BUFFER_SIZE - tx_buffer_end;
		if (tx_buffer_start == 0) maxCopySize--;
		if (maxCopySize > dataSize) {
			for (i = 0; i < dataSize; i++) tx_buffer[tx_buffer_end++] = udata[i];
			sentSize += dataSize;
			dataSize = 0;
		} else {
			for (i = 0; i < maxCopySize; i++) tx_buffer[tx_buffer_end + i] = udata[i];
			tx_buffer_end = 0;
			dataSize -= maxCopySize;
			udata += maxCopySize;
			sentSize += maxCopySize;
		}
	}
	for (i = 0; i < dataSize && tx_buffer_end + 1 < tx_buffer_start; i++) {
		tx_buffer[tx_buffer_end++] = udata[i];
	}
	transmitData();
	if (iflag) sti();
	return sentSize + i;
}

int serialReceiveData(void* data, int dataSize) {
	unsigned char* udata = data;
	int i;
	int recvSize = 0;
	int iflag = getIF();
	if (dataSize <= 0) return 0;
	if (!initialized) {
		for (i = 0; i < dataSize; i++) {
			int c = getchar_serial_direct_nowait();
			if (c < 0) return i;
			udata[i] = (unsigned char)c;
		}
		return dataSize;
	}
	cli();
	if (rx_buffer_end < rx_buffer_start) {
		int maxCopySize = RX_BUFFER_SIZE - rx_buffer_start;
		if (maxCopySize > dataSize) {
			for (i = 0; i < dataSize; i++) udata[i] = rx_buffer[rx_buffer_start++];
			recvSize += dataSize;
			dataSize = 0;
		} else {
			for (i = 0; i < maxCopySize; i++) udata[i] = rx_buffer[rx_buffer_start + i];
			rx_buffer_start = 0;
			dataSize -= maxCopySize;
			udata += maxCopySize;
			recvSize += maxCopySize;
		}
	}
	for (i = 0; i < dataSize && rx_buffer_start < rx_buffer_end; i++) {
		udata[i] = rx_buffer[rx_buffer_start++];
	}
	if (iflag) sti();
	return recvSize + i;
}

int serialGetTxBufferAvailable(void) {
	if (initialized) {
		int res = tx_buffer_start - 1 - tx_buffer_end;
		if (res < 0) res += TX_BUFFER_SIZE;
		return res;
	} else {
		return 0;
	}
}

int serialGetRxBufferAvailable(void) {
	if (initialized) {
		int res = rx_buffer_end - rx_buffer_start;
		if (res < 0) res += RX_BUFFER_SIZE;
		return res;
	} else {
		return 0;
	}
}

void serialSendDataBlocking(const void* data, int dataSize) {
	const unsigned char* udata = data;
	int leftSize = dataSize;
	while (leftSize > 0) {
		int sentSize = serialSendData(udata, leftSize);
		leftSize -= sentSize;
		udata += sentSize;
	}
}

void serialReceiveDataBlocking(void* data, int dataSize) {
	unsigned char* udata = data;
	int leftSize = dataSize;
	while (leftSize > 0) {
		int recvSize = serialReceiveData(udata, leftSize);
		leftSize -= recvSize;
		udata += recvSize;
		if (!getIF()) receiveData();
	}
}

int serialPutchar(int c) {
	unsigned char uc = (unsigned char)c;
	serialSendDataBlocking(&uc, 1);
	return uc;
}

int serialGetchar(void) {
	unsigned char uc;
	serialReceiveDataBlocking(&uc, 1);
	return uc;
}

int serialGetcharNowait(void) {
	unsigned char uc;
	int res = serialReceiveData(&uc, 1);
	return res >= 1 ? uc : -1;
}

static int printfSerialCallback(const char* str, int str_len, void* status) {
	int i;
	(void)status;
	for (i = 0; i < str_len; i++) {
		if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r') && serialPutchar('\r') < 0) return 0;
		if (serialPutchar(str[i]) < 0) return 0;
	}
	return 1;
}

int vprintfSerial(const char* format, va_list args) {
	return my_vprintf_base(printfSerialCallback, 0, format, args);
}

int printfSerial(const char* format, ...) {
	va_list vl;
	int ret;
	va_start(vl, format);
	ret = vprintfSerial(format, vl);
	va_end(vl);
	return ret;
}
