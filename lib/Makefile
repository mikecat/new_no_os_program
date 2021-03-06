CC=gcc
CFLAGS=-m32 -fno-leading-underscore -Wall -Wextra -O2
CFLAGS_D=-m32 -fno-leading-underscore -Wall -Wextra -Og -g3
AR=ar
RM=rm -f
TARGET=libnnop.a
TARGET_D=libnnop_d.a

OBJS=serial_direct.o my_printf.o entry_paging.o pages.o panic.o display.o initialize.o \
	call_uefi.o acpi.o uefi_printf.o interrupt_vector.o interrupt.o font.o memory_utils.o \
	text_display.o serial.o timer.o

OBJS_D=$(join $(addsuffix _d,$(basename $(OBJS))),$(suffix $(OBJS)))

.PHONY: release
release: $(TARGET)

.PHONY: debug
debug: $(TARGET_D)

.PHONY: all
all: release debug

$(TARGET): $(OBJS)
	$(RM) $(TARGET)
	$(AR) -csr $(TARGET) $(OBJS)

$(TARGET_D): $(OBJS_D)
	$(RM) $(TARGET_D)
	$(AR) -csr $(TARGET_D) $(OBJS_D)

%_d.o: %.c
	$(CC) -c $(CFLAGS_D) -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

%_d.o: %.S
	$(CC) -c $(CFLAGS_D) -o $@ $^

%.o: %.S
	$(CC) -c $(CFLAGS) -o $@ $^

%.o: %.asm
	nasm -f coff -o $@ $^

%_d.o: %.asm
	nasm -f coff -o $@ $^

interrupt_vector.S: interrupt_vector_gen.pl
	perl $^ > $@

font.c: font_gen.pl font.bmp
	perl $< > $@

.PHONY: clean
clean: clean-release clean-debug

.PHONY: clean-release
clean-release:
	$(RM) $(TARGET) $(OBJS) interrupt_vector.S font.c

.PHONY: clean-debug
clean-debug:
	$(RM) $(TARGET_D) $(OBJS_D) interrupt_vector.S font.c
