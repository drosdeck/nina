# nina
nina is the simplified acpi

#how to

copiar o diretorio src para drivers e renomear como nina
em driver Makefile adicionar

obj-$(CONFIG_NINA)   +=nina/

e no arquivo drivers/Kconfig adicionar

source "drivers/nina/Kconfig"

necessario comentar  no arquivo 

drivers/acpi/bus.c

na funcao 

void __init acpi_early_init(void)

comentar 

if (acpi_disabled)
		return;


remover acpi_disabled das funcoes abaixo

acpi_boot_table_init  /arch/x86/kernel/acpi/boot.c
early_acpi_boot_init /arch/x86/kernel/acpi/boot.c
acpi_table_parse /drivers/acpi/tables.c
acpi_match_platform_list    /drivers/acpi/utils.c
 acpi_table_parse_entries_array  /drivers/acpi/tables.c
 acpi_boot_init   /arch/x86/kernel/acpi/boot.c
acpi_parse_spcr    /drivers/acpi/spcr.c
acpi_subsystem_init  /drivers/acpi/bus.c

