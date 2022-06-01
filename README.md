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
