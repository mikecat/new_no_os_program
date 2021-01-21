#include "serial_direct.h"
#include "io_macro.h"

void entry(unsigned int dummy, unsigned int* first_table) {
	static const unsigned int guid_target[] = {
		0x8868e871u, 0x11d3e4f1u, 0x800022bcu, 0x81883cc7u
	};
	const unsigned int fadt_target = 0x50434146;
	int guid_num = (int)first_table[16];
	unsigned int* guid_table = (unsigned int*)first_table[17];
	int i;
	unsigned int* rsdp = 0;
	unsigned int* rsdt = 0;
	unsigned int* fadt = 0;
	unsigned int* dsdt = 0;
	unsigned int smi_cmd = 0, acpi_enable = 0, pm1a = 0, pm1b = 0;
	unsigned char* aml = 0;
	int aml_size = 0;
	int s5_offset = -1;
	int pm1a_value = 0, pm1b_value = 0;
	int s5_length = 0, s5_limit = 0;
	int in_data;

	printf_serial_direct("# elements of first table : %d\n", guid_num);
	printf_serial_direct("first tabie : %p\n", (void*)guid_table);
	for (i = 0; i < guid_num; i++) {
		printf_serial_direct("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X} : 0x%08x\n",
			guid_table[i * 5],
			guid_table[i * 5 + 1] & 0xffff, (guid_table[i * 5 + 1] >> 16) & 0xffff,
			guid_table[i * 5 + 2] & 0xff, (guid_table[i * 5 + 2] >> 8) & 0xff,
			(guid_table[i * 5 + 2] >> 16) & 0xff, (guid_table[i * 5 + 2] >> 24) & 0xff,
			guid_table[i * 5 + 3] & 0xff, (guid_table[i * 5 + 3] >> 8) & 0xff,
			(guid_table[i * 5 + 3] >> 16) & 0xff, (guid_table[i * 5 + 3] >> 24) & 0xff,
			guid_table[i * 5 + 4]);
		if (guid_table[i * 5] == guid_target[0] && guid_table[i * 5 + 1] == guid_target[1] &&
		guid_table[i * 5 + 2] == guid_target[2] && guid_table[i * 5 + 3] == guid_target[3]) {
			rsdp = (unsigned int*)guid_table[i * 5 + 4];
		}
	}
	printf_serial_direct("\n");

	if (rsdp == 0) {
		printf_serial_direct("RSDP not found!\n");
		for(;;);
	}
	printf_serial_direct("RSDP : %p\n", (void*)rsdp);
	rsdt = (unsigned int*)rsdp[4];

	printf_serial_direct("RSDT : %p\n", (void*)rsdt);
	for (i = 9; i * 4 < (int)rsdt[1]; i++) {
		if (*(unsigned int*)rsdt[i] == fadt_target) {
			fadt = (unsigned int*)rsdt[i];
		}
	}

	if (fadt == 0) {
		printf_serial_direct("FADT not found!\n");
		for(;;);
	}
	printf_serial_direct("FADT : %p\n", (void*)fadt);
	dsdt = (unsigned int*)fadt[10];
	smi_cmd = fadt[12];
	acpi_enable = fadt[13] & 0xff;
	pm1a = fadt[16];
	pm1b = fadt[17];
	printf_serial_direct("SMI_CMD : 0x%04x\n", smi_cmd);
	printf_serial_direct("ACPI_ENABLE : 0x%02x\n", acpi_enable);
	printf_serial_direct("PM1a_CNT_BLK : 0x%04x\n", pm1a);
	printf_serial_direct("PM1b_CNT_BLK : 0x%04x\n", pm1b);

	printf_serial_direct("\n");
	printf_serial_direct("DSDT: %p\n", (void*)dsdt);
	aml = (unsigned char*)(dsdt + 9);
	aml_size = dsdt[1] - 36;
	for (i = 5; i < aml_size; i++) {
		if (aml[i - 5] == 0x08 && aml[i - 4] == 0x5f && aml[i - 3] == 0x53 &&
		aml[i - 2] == 0x35 && aml[i - 1] == 0x5f && aml[i] == 0x12) {
			if (s5_offset == -1) s5_offset = i + 1; else s5_offset = -2;
		}
	}
	if (s5_offset == -1) {
		printf_serial_direct("_S5_ not found!\n");
		for(;;);
	}
	if (s5_offset == -2) {
		printf_serial_direct("multiple _S5_ found!\n");
		for(;;);
	}

	if (s5_offset >= aml_size) {
		printf_serial_direct("_S5_ out of range!\n");
		for(;;);
	}
	if (aml[s5_offset] >= 0x40) {
		printf_serial_direct("_S5_ unsupported length!\n");
		for(;;);
	}
	s5_length = aml[s5_offset];
	s5_offset++;
	s5_limit = s5_offset + s5_length;
	if (s5_limit > aml_size) {
		printf_serial_direct("_S5_ too long!\n");
		for(;;);
	}

	if (aml[s5_offset] < 2) {
		printf_serial_direct("_S5_ too few elements!\n");
		for(;;);
	}
	s5_offset++;

	if (s5_offset >= s5_limit || (aml[s5_offset] == 0x0a && s5_offset + 1 >= s5_limit)) {
		printf_serial_direct("_S5_ first value out-of-range!\n");
		for(;;);
	}
	if (aml[s5_offset] == 0) {
		pm1a_value = 0;
		s5_offset++;
	} else if (aml[s5_offset] == 0x0a) {
		pm1a_value = aml[s5_offset + 1];
		s5_offset+=2;
	} else {
		printf_serial_direct("_S5_ first value unsupported!\n");
		for(;;);
	}

	if (s5_offset >= s5_limit || (aml[s5_offset] == 0x0a && s5_offset + 1 >= s5_limit)) {
		printf_serial_direct("_S5_ second value out-of-range!\n");
		for(;;);
	}
	if (aml[s5_offset] == 0) {
		pm1b_value = 0;
		s5_offset++;
	} else if (aml[s5_offset] == 0x0a) {
		pm1b_value = aml[s5_offset + 1];
		s5_offset++;
	} else {
		printf_serial_direct("_S5_ second value unsupported!\n");
		for(;;);
	}

	printf_serial_direct("value for PM1a : 0x%x\n", pm1a_value);
	printf_serial_direct("value for PM1b : 0x%x\n", pm1b_value);
	printf_serial_direct("\n");

	in16(in_data, pm1a);
	printf_serial_direct("value of PM1a_CNT : 0x%04x\n", in_data);

	printf_serial_direct("sending ACPI_ENABLE to SMI_CMD\n");
	out8(acpi_enable, smi_cmd);
	do {
		in16(in_data, pm1a);
	} while (!(in_data & 1));
	printf_serial_direct("value of PM1a_CNT : 0x%04x\n", in_data);

	printf_serial_direct("\nplease enter \"p\" (without quote) to proceed\n");
	while (getchar_serial_direct() != 'p');

	printf_serial_direct("\nsetting SLP_TYP and SLP_EN\n");
	in16(in_data, pm1a);
	out16((in_data & ~(7 << 10)) | ((pm1a_value & 7) << 10) | (1 << 13), pm1a);
	if (pm1b != 0) {
		in16(in_data, pm1b);
		out16((in_data & ~(7 << 10)) | ((pm1a_value & 7) << 10) | (1 << 13), pm1b);
	}

	printf_serial_direct("finished.\n");
	for(;;);
}
