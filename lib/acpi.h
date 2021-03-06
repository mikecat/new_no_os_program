#ifndef ACPI_H_GUARD_2FE58A77_1827_4901_B71E_4C1F75B54538
#define ACPI_H_GUARD_2FE58A77_1827_4901_B71E_4C1F75B54538

#include "regs.h"

/*
ACPI操作を初期化する
成功したら非0、失敗したら0を返す
*/
int initAcpi(struct initial_regs* regs);

/*
RSDPのアドレスを指定し、ACPI操作を初期化する
成功したら非0、失敗したら0を返す
*/
int initAcpiWithRsdp(unsigned int* rsdp);

/*
ACPIを有効化する
成功したら非0、失敗したら0を返す
*/
int enableAcpi(void);

/*
ACPIを無効化する
成功したら非0、失敗したら0を返す
*/
int disableAcpi(void);

/*
ACPIが有効になっているかをチェックする。
有効の場合は非0、無効(エラーを含む)の場合は0を返す
*/
int acpiEnabled(void);

/*
ACPIにより電源を切る
成功したら非0、失敗したら0を返す
(成功したら電源が切れるはずなので、非0が返るのは大失敗という可能性も…)
*/
int acpiPoweroff(void);

#endif
