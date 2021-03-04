#ifndef CALL_UEFI_H_GUARD_6AD38789_C1A6_417D_B32A_9C85C9B3DC49
#define CALL_UEFI_H_GUARD_6AD38789_C1A6_417D_B32A_9C85C9B3DC49

/*
UEFIの関数を呼ぶ。
事前にスタックをidentity-mappedな場所に移しておくこと。
また、ページングはプログラムデータがidentitiy-mappedになるような状態にしておくこと。
*/
int call_uefi(struct initial_regs* regs, void* function,
	unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4, unsigned int arg5);

/* スタックをidentity-mappedな場所に移し、関数funcを呼ぶ。 */
int callWithIdentStack(int(*func)(void* data), void* data);

#endif
