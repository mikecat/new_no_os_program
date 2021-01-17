#include <stdarg.h>
#include "my_printf.h"

static char* uint_to_str(unsigned int value, char* buffer_end, int base, int is_upper) {
	char* p = buffer_end;
	if (value == 0) {
		p--;
		*p = '0';
		return p;
	} else {
		while (value > 0) {
			int cur = value % base;
			value /= base;
			p--;
			if (cur < 10) *p = '0' + cur;
			else if (is_upper) *p = 'A' + (cur - 10);
			else *p = 'a' + (cur - 10);
		}
		return p;
	}
}

static int str_len(const char* s) {
	int c = 0;
	while (*s) {
		c++;
		s++;
	}
	return c;
}

static int my_printf_pad(int *count, my_printf_base_callback callback, void* callback_status, int current_len, int target_len, const char* pad_buf, int pad_buf_len) {
	int i;
	for (i = current_len; i < target_len; ) {
		int delta = target_len - i;
		if (delta > pad_buf_len) delta = pad_buf_len;
		if (!callback(pad_buf, delta, callback_status)) return 0;
		i += delta;
		(*count) += delta;
	}
	return 1;
}

int my_vprintf_base(my_printf_base_callback callback, void* callback_status, const char* format, va_list args) {
	const char* p = format;
	char int_buffer[16];
	char* int_buffer_end = int_buffer + sizeof(int_buffer) / sizeof(*int_buffer);
	static const char zeros[8] = "00000000", spaces[8] = "        ", minus = '-';
	const int zeros_size = sizeof(zeros) / sizeof(*zeros);
	const int spaces_size = sizeof(spaces) / sizeof(*spaces);
	int count = 0;
	while (*p != '\0') {
		if (*p == '%') {
			char* start = 0;
			int length = 0;
			int zero_pad = 0;
			p++;
			if (*p == '0') {
				zero_pad = 1;
				p++;
			}
			while ('0' <= *p && *p <= '9') {
				length = length * 10 + (*p - '0');
				p++;
			}
			if (*p == '\0') {
				if (!callback(start, p - start, callback_status)) return -1;
				return count + (p - start);
			}
			switch (*p) {
				case '%':
					if (!callback(p, 1, callback_status)) return -1;
					count++;
					break;
				case 'c':
					{
						int value = va_arg(args, int);
						char vchar = (char)value;
						my_printf_pad(&count, callback, callback_status,
							1, length, spaces, spaces_size);
						if (!callback(&vchar, 1, callback_status)) return -1;
						count++;
					}
					break;
				case 's':
					{
						const char* pstr = va_arg(args, const char*);
						int slen = str_len(pstr);
						my_printf_pad(&count, callback, callback_status,
							slen, length, spaces, spaces_size);
						if (pstr != 0 && !callback(pstr, slen, callback_status)) return -1;
						count += slen;
					}
					break;
				case 'd': case 'i':
					{
						int value = va_arg(args, int);
						unsigned int uvalue = value < 0 ? -value : value;
						char* vstr = uint_to_str(uvalue, int_buffer_end, 10, 0);
						int vlen = int_buffer_end - vstr;
						if (vlen < length) {
							if (zero_pad) {
								if (value < 0) {
									if (!callback(&minus, 1, callback_status)) return -1;
									count++;
								}
								my_printf_pad(&count, callback, callback_status,
									vlen + (value < 0), length, zeros, zeros_size);

							} else {
								my_printf_pad(&count, callback, callback_status,
									vlen + (value < 0), length, spaces, spaces_size);
								if (value < 0) {
									if (!callback(&minus, 1, callback_status)) return -1;
									count++;
								}
							}
						} else {
							if (value < 0 && !callback(&minus, 1, callback_status)) return -1;
							count++;
						}
						if (!callback(vstr, vlen, callback_status)) return -1;
						count += vlen;
					}
					break;
				case 'o': case 'u': case 'x': case 'X': case 'p':
					{
						unsigned int value = *p == 'p' ? (unsigned int)va_arg(args, void*) : va_arg(args, unsigned int);
						int base = *p == 'u' ? 10 : (*p == 'o' ? 8 : 16);
						char* vstr = uint_to_str(value, int_buffer_end, base, *p != 'x');
						int vlen = int_buffer_end - vstr;
						if (zero_pad) {
							my_printf_pad(&count, callback, callback_status,
								vlen, length, zeros, zeros_size);
						} else {
							my_printf_pad(&count, callback, callback_status,
								vlen, length, spaces, spaces_size);
						}
						if (!callback(vstr, vlen, callback_status)) return -1;
						count += vlen;
					}
					break;
				default:
					if (!callback(start, (p + 1) - start, callback_status)) return -1;
					count += (p + 1) - start;
					break;
			}
			p++;
		} else {
			const char* start = p;
			while (*p != '%' && *p != '\0') p++;
			if (!callback(start, p - start, callback_status)) return -1;
			count += p - start;
		}
	}
	return count;
}

int my_printf_base(my_printf_base_callback callback, void* callback_status, const char* format, ...) {
	va_list vl;
	int ret;
	va_start(vl, format);
	ret = my_vprintf_base(callback, callback_status, format, vl);
	va_end(vl);
	return ret;
}

struct my_snprintf_info {
	char* buffer;
	int buffer_left;
};

static int my_snprintf_callback(const char* data, int length, void* status) {
	struct my_snprintf_info* info = status;
	int i;
	for (i = 0; i < length && info->buffer_left > 0; i++) {
		*(info->buffer++) = data[i];
		info->buffer_left--;
	}
	return 1;
}

int my_snprintf(char* out, int limit, const char* format, ...) {
	struct my_snprintf_info info = {out, limit - 1};
	int ret;
	va_list vl;
	if (out == 0) info.buffer_left = 0;
	va_start(vl, format);
	ret = my_vprintf_base(my_snprintf_callback, &info, format, vl);
	va_end(vl);
	if (info.buffer != 0) *info.buffer = '\0';
	return ret;
}
