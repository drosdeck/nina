#ifndef _NINASCAN_H_
#define _NINASCAN_H_


void __init nina_scan_init(void);

void nina_free_pnp_ids(struct acpi_device_pnp *pnp);

void nina_device_add_finalize(struct acpi_device *device);

void nina_init_device_object(struct acpi_device *device, acpi_handle handle,
			     int type);
int nina_device_add(struct acpi_device *adev);
#endif
