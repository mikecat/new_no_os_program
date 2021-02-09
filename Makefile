CC=gcc
CFLAGS=-Ilib -Llib -nostdlib -Wall -Wextra -O2
CFLAGS_D=-Llib -Ilib -nostdlib -Wall -Wextra -Og -g3
RM=rm -f

ENTRY_NO_PAGING=_entry
ENTRY_PAGING=_entry_paging

IMAGE_BASE_NO_PAGING=0x00400000
IMAGE_BASE_PAGING=0xc0000000

TARGETS_NO_PAGING= \
	01_first.img \
	03_serial.img \
	04_tanken.img 04_tanken_2.img 04_tanken_mmap.img 04_tanken_mmap_2.img \
	05_poweroff.img \
# --- END_OF_TARGETS ---

TARGETS_PAGING= \
	06_paging.img \
# --- END_OF_TARGETS ---

TARGETS=$(TARGETS_NO_PAGING) $(TARGETS_PAGING)

TARGETS_D=$(join $(addsuffix _d,$(basename $(TARGETS))),$(suffix $(TARGETS)))

.PHONY: release
release: $(TARGETS)

.PHONY: debug
debug: $(TARGETS_D)

.PHONY: all
all: release debug

%_d.img: %.c lib/libnnop_d.a
	$(CC) -e $(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix _d.img,$(basename $(TARGETS_NO_PAGING))))")),$(ENTRY_NO_PAGING) $(ENTRY_PAGING)) \
		-Wl,--image-base=$(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(addsuffix _d.img,$(basename $(TARGETS_NO_PAGING))))")),$(IMAGE_BASE_NO_PAGING) $(IMAGE_BASE_PAGING)) \
		$(CFLAGS_D) -o $*_d.exe $< -lnnop_d
	./exe2img.pl $*_d.exe $@

%.img: %.c lib/libnnop.a
	$(CC) -e $(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(TARGETS_NO_PAGING))")),$(ENTRY_NO_PAGING) $(ENTRY_PAGING)) \
		-Wl,--image-base=$(word $(patsubst "%",1,$(subst "",2,"$(filter $@,$(TARGETS_NO_PAGING))")),$(IMAGE_BASE_NO_PAGING) $(IMAGE_BASE_PAGING)) \
		$(CFLAGS) -o $*.exe $< -lnnop
	./exe2img.pl $*.exe $@

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
