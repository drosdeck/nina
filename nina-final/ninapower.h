#ifndef _NINAPOWER_H_
#define _NINAPOWER_H_

int nina_extract_power_resources(union acpi_object *package, unsigned int start,
				 struct list_head *list);

#endif
