# nina
nina is the simplified acpi

#how to

copiar o diretorio src para drivers e renomear como nina
em driver Makefile adicionar

obj-$(CONFIG_NINA)   +=nina/

e no arquivo drivers/Kconfig adicioanar

source "drivers/nina/Kconfig
