#ifndef _NINABUS_H_
#define _NINABUS_H_


extern const struct fwnode_operations acpi_device_fwnode_ops;

#define ACPI_STA_DEFAULT (ACPI_STA_DEVICE_PRESENT | ACPI_STA_DEVICE_ENABLED | \
			  ACPI_STA_DEVICE_UI | ACPI_STA_DEVICE_FUNCTIONING)

#endif
