CC=gcc
CFLAGS=-Wall -Wextra -O2
AR=ar
RM=rm -f
TARGET=libnnop.a

OBJS=serial_direct.o my_printf.o

$(TARGET): $(OBJS)
	$(RM) $(TARGET)
	$(AR) -csr $(TARGET) $(OBJS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)
