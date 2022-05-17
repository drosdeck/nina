#ifndef _NINABUS_H_
#define _NINABUS_H_


extern const struct fwnode_operations acpi_device_fwnode_ops;

extern struct list_head nina_bus_id_list;

#define ACPI_MAX_DEVICE_INSTANCES	4096

struct acpi_device_bus_id {
	const char *bus_id;
	struct ida instance_ida;
	struct list_head node;
};


#define ACPI_STA_DEFAULT (ACPI_STA_DEVICE_PRESENT | ACPI_STA_DEVICE_ENABLED | \
			  ACPI_STA_DEVICE_UI | ACPI_STA_DEVICE_FUNCTIONING)

#endif
