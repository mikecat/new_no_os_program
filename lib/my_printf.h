#ifndef MY_PRINTF_H_GUARD_6AAFBB5B_274A_4188_B744_F6D44CCCADB1
#define MY_PRINTF_H_GUARD_6AAFBB5B_274A_4188_B744_F6D44CCCADB1

/*
put str_len bytes from str to buffer
return non-zero on success, zero on failure
*/
typedef int(*my_printf_base_callback)(const char* str, int str_len, void* status);

int my_vprintf_base(my_printf_base_callback callback, void* callback_status, const char* format, va_list args);
int my_printf_base(my_printf_base_callback callback, void* callback_status, const char* format, ...);
int my_snprintf(char* out, int limit, const char* format, ...);

#endif
