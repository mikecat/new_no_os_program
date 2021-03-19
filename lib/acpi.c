#include "acpi.h"
#include "io_macro.h"
#include "serial_direct.h"

static const unsigned int guidTarget[] = {
	0x8868e871u, 0x11d3e4f1u, 0x800022bcu, 0x81883cc7u
};

static const unsigned char s5Target[] = {0x08, 0x5f, 0x53, 0x35, 0x5f, 0x12};

static int initialized = 0;

static unsigned int* rsdp = 0;
static unsigned int* rsdt = 0, *xsdt = 0;
static unsigned int* fadt = 0;
static unsigned int* dsdt = 0;
static unsigned int smi_cmd = 0, acpi_enable = 0, acpi_disable = 0;
static unsigned int pm1a_cnt_blk = 0, pm1b_cnt_blk = 0;
static unsigned char* aml = 0;
static int aml_size = 0;
static int pm1a_value = 0, pm1b_value = 0;

static unsigned char* searchPuttern(unsigned char* data, const unsigned char* target,
unsigned int dataSize, unsigned int targetSize) {
	unsigned int i, j;
	for (i = targetSize; i <= dataSize; i++) {
		int yes = 1;
		for (j = 0; j < targetSize; j++) {
			if (data[i - targetSize + j] != target[j]) {
				yes = 0;
				break;
			}
		}
		if (yes) return data + (i - targetSize);
	}
	return 0;
}

int initAcpi(struct initial_regs* regs) {
	unsigned int* guidTable;
	unsigned int guidTableNumElements;
	unsigned int guidTableElementSize;
	unsigned int i;
	if (regs->efer & 0x400) {
		unsigned int* firstTable = (unsigned int*)regs->iregs.edx;
		guidTable = (unsigned int*)firstTable[28];
		guidTableNumElements = firstTable[26];
		guidTableElementSize = 6;
	} else {
		unsigned int* firstTable = (unsigned int*)((unsigned int*)regs->iregs.esp)[2];
		guidTable = (unsigned int*)firstTable[17];
		guidTableNumElements = firstTable[16];
		guidTableElementSize = 5;
	}
	for (i = 0; i < guidTableNumElements; i++) {
		unsigned int* element = guidTable + guidTableElementSize * i;
		if (element[0] == guidTarget[0] && element[1] == guidTarget[1] &&
		element[2] == guidTarget[2] && element[3] == guidTarget[3]) {
			return initAcpiWithRsdp((unsigned int*)element[4]);
		}
	}
	printf_serial_direct("ACPI: RSDP not found\n");
	initialized = 0;
	return 0;
}

int initAcpiWithRsdp(unsigned int* rsdp_arg) {
	unsigned int i;
	unsigned char* s5;
	unsigned int s5_size, s5_data_size, s5_offset, s5_got[2];
	initialized = 0;
	rsdp = rsdp_arg;
	rsdt = (unsigned int*)rsdp[4];
	xsdt = (unsigned int*)(rsdp[7] == 0 ? rsdp[6] : 0);
	fadt = getAcpiTable("FACP", 0);
	if (fadt == 0) {
		printf_serial_direct("ACPI: FADT not found\n");
		return 0;
	}
	dsdt = (unsigned int*)fadt[10];
	smi_cmd = fadt[12];
	acpi_enable = fadt[13] & 0xff;
	acpi_disable = (fadt[13] >> 8) & 0xff;
	pm1a_cnt_blk = fadt[16];
	pm1b_cnt_blk = fadt[17];
	if (dsdt[1] < 36) {
		printf_serial_direct("ACPI: DSDT too small\n");
		return 0;
	}
	aml = (unsigned char*)&dsdt[9];
	aml_size = dsdt[1] - 36;
	s5 = searchPuttern(aml, s5Target, aml_size, sizeof(s5Target));
	if (s5 == 0) {
		printf_serial_direct("ACPI: _S5_ not found\n");
		return 0;
	}
	s5_size = aml_size - (s5 - aml);
	if (searchPuttern(s5 + 1, s5Target, s5_size - 1, sizeof(s5Target)) != 0) {
		printf_serial_direct("ACPI: multiple _S5_ found\n");
		return 0;
	}
	if (s5_size < 7) {
		printf_serial_direct("ACPI: _S5_ too small\n");
		return 0;
	}
	s5_data_size = s5[6];
	if (s5_data_size >= 0x40) {
		printf_serial_direct("ACPI: _S5_ data too big\n");
		return 0;
	}
	if (s5_data_size > s5_size - 6) {
		printf_serial_direct("ACPI: _S5_ data out-of-range\n");
		return 0;
	}
	if (s5[7] < 2) {
		printf_serial_direct("ACPI: _S5_ too few elements\n");
		return 0;
	}
	s5_offset = 2;
	for (i = 0; i < 2; i++) {
		if (s5_offset >= s5_data_size) {
			printf_serial_direct("ACPI: _S5_ unexpected end of data\n");
			return 0;
		}
		if (s5[6 + s5_offset] == 0x00) {
			s5_got[i] = 0;
			s5_offset++;
		} else if (s5[6 + s5_offset] == 0x0a) {
			if (++s5_offset >= s5_data_size) {
				printf_serial_direct("ACPI: _S5_ unexpected end of data\n");
				return 0;
			}
			s5_got[i] = s5[6 + s5_offset];
			s5_offset++;
		} else {
			printf_serial_direct("ACPI: _S5_ unsupported data\n");
			return 0;
		}
	}
	pm1a_value = s5_got[0];
	pm1b_value = s5_got[1];
	initialized = 1;
	return 1;
}

int countAcpiTable(const char* target) {
	unsigned int target_int = 0;
	char* t = (char*)&target_int;
	unsigned int i;
	int count = 0;
	for (i = 0; i < 4 && i < (int)sizeof(target_int); i++) {
		t[i] = target[i];
	}
	if (xsdt != 0) {
		for(i = 9; 4 * i + 8 <= xsdt[1]; i += 2) {
			if (xsdt[i + 1] == 0) {
				if (*(unsigned int*)xsdt[i] == target_int) {
					count++;
				}
			}
		}
	} else {
		for(i = 9; 4 * i + 4 <= rsdt[1]; i++) {
			if (*(unsigned int*)rsdt[i] == target_int) {
				count++;
			}
		}
	}
	return count;
}

unsigned int* getAcpiTable(const char* target, int which) {
	unsigned int target_int = 0;
	char* t = (char*)&target_int;
	unsigned int i;
	int count = 0;
	for (i = 0; i < 4 && i < (int)sizeof(target_int); i++) {
		t[i] = target[i];
	}
	if (xsdt != 0) {
		for(i = 9; 4 * i + 8 <= xsdt[1]; i += 2) {
			if (xsdt[i + 1] == 0) {
				if (*(unsigned int*)xsdt[i] == target_int) {
					if (count == which) return (unsigned int*)xsdt[i];
					count++;
				}
			}
		}
	} else {
		for(i = 9; 4 * i + 4 <= rsdt[1]; i++) {
			if (*(unsigned int*)rsdt[i] == target_int) {
				if (count == which) return (unsigned int*)rsdt[i];
				count++;
			}
		}
	}
	return 0;
}

int enableAcpi(void) {
	int status;
	if (!initialized) return 0;
	if (acpiEnabled()) return 1;
	if (smi_cmd == 0 || acpi_enable == 0) return 0;
	out8(acpi_enable, smi_cmd);
	do {
		in16(status, pm1a_cnt_blk);
	} while (!(status & 1));
	return 1;
}

int disableAcpi(void) {
	int status;
	if (!initialized) return 0;
	if (!acpiEnabled()) return 1;
	if (smi_cmd == 0 || acpi_disable == 0) return 0;
	out8(acpi_disable, smi_cmd);
	do {
		in16(status, pm1a_cnt_blk);
	} while (status & 1);
	return 1;
}

int acpiEnabled(void) {
	int status;
	if (!initialized) return 0;
	in16(status, pm1a_cnt_blk);
	return (status & 1) != 0;
}

int acpiPoweroff(void) {
	int status;
	if (!initialized) return 0;
	if (!enableAcpi()) return 0;
	in16(status, pm1a_cnt_blk);
	out16((status & ~(7 << 10)) | ((pm1a_value & 7) << 10) | (1 << 13), pm1a_cnt_blk);
	if (pm1b_cnt_blk != 0) {
		in16(status, pm1b_cnt_blk);
		out16((status & ~(7 << 10)) | ((pm1b_value & 7) << 10) | (1 << 13), pm1b_cnt_blk);
	}
	return 1;
}
