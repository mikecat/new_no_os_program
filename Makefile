CC=gcc
CFLAGS=-Ilib -Llib -m32 -nostdlib -fno-leading-underscore -Wall -Wextra -O2
CFLAGS_D=-Llib -Ilib -m32 -nostdlib -fno-leading-underscore -Wall -Wextra -Og -g3
RM=rm -f

ENTRY_NO_PAGING=entry
ENTRY_PAGING=entry_paging

IMAGE_BASE_NO_PAGING=0x00400000
IMAGE_BASE_PAGING=0xc0000000

TARGETS_NO_PAGING= \
	01_first.img \
	03_serial.img \
	04_tanken.img 04_tanken_2.img 04_tanken_mmap.img 04_tanken_mmap_2.img \
	05_poweroff.img 08_x64_test.img 09_display_list.img 09_display.img \
	08_x64_test_2.img 10_memorymap.img 05_poweroff_lib_no_paging.img \
	11_conout.img \
# --- END_OF_TARGETS ---

TARGETS_PAGING= \
	07_paging.img 09_display_print_gop.img 09_display_lib.img 05_poweroff_lib.img \
	11_uefi_printf_test.img 12_madt.img 12_divzero.img 12_timer_device_test.img \
	12_madt_uefi_print.img 12_serial_test.img \
# --- END_OF_TARGETS ---

TARGETS=$(TARGETS_NO_PAGING) $(TARGETS_PAGING)

TARGETS_D=$(join $(addsuffix _d,$(basename $(TARGETS))),$(suffix $(TARGETS)))

TARGETS_X64=$(join $(addsuffix _x64,$(basename $(TARGETS))),$(suffix $(TARGETS)))

TARGETS_D_X64=$(join $(addsuffix _d_x64,$(basename $(TARGETS))),$(suffix $(TARGETS)))

.PHONY: release-x86
release-x86: $(TARGETS)

.PHONY: release-x64
release-x64: $(TARGETS_X64)

.PHONY: debug-x86
debug-x86: $(TARGETS_D)

.PHONY: debug-x64
debug-x64: $(TARGETS_D_X64)

.PHONY: release
release: release-x86 release-x64

.PHONY: debug
debug: debug-x86 debug-x64

.PHONY: all
all: release debug

.PRECIOUS: %.exe %_d.exe %_x64.exe %.efi

%_x64.img: %_x64.efi
	./file2img.pl $< $@ BOOTX64.EFI

%.img: %.efi
	./file2img.pl $< $@ BOOTIA32.EFI

%.efi: %.exe
	./exe2efi.pl $< $@

%_x64.exe: %.exe
	./exe_tox64.pl $< $@

%_d.exe: %.c lib/libnnop_d.a
	$(CC) -e $(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix _d.exe,$(basename $(TARGETS_NO_PAGING))))")),$(ENTRY_NO_PAGING) $(ENTRY_PAGING)) \
		-Wl,--image-base=$(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix _d.exe,$(basename $(TARGETS_NO_PAGING))))")),$(IMAGE_BASE_NO_PAGING) $(IMAGE_BASE_PAGING)) \
		$(CFLAGS_D) -o $*_d.exe $< -lnnop_d

%.exe: %.c lib/libnnop.a
	$(CC) -e $(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix .exe,$(basename $(TARGETS_NO_PAGING))))")),$(ENTRY_NO_PAGING) $(ENTRY_PAGING)) \
		-Wl,--image-base=$(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix .exe,$(basename $(TARGETS_NO_PAGING))))")),$(IMAGE_BASE_NO_PAGING) $(IMAGE_BASE_PAGING)) \
		$(CFLAGS) -o $*.exe $< -lnnop

lib/libnnop_d.a:
	make -C lib debug

lib/libnnop.a:
	make -C lib release

.PHONY: clean
clean: clean-release clean-debug

.PHONY: clean-release
clean-release:
	$(RM) $(TARGETS) $(addsuffix .exe,$(basename $(TARGETS)))
	make -C lib clean-release

.PHONY: clean-debug
clean-debug:
	$(RM) $(TARGETS_D) $(addsuffix .exe,$(basename $(TARGETS_D)))
	make -C lib clean-debug
