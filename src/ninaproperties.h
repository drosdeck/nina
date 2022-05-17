#ifndef _NINAPROPERTIES_H_
#define _NINAPROPERTIES_H_

#define ACPI_DT_NAMESPACE_HID	"PRP0001"

static bool acpi_extract_properties(const union acpi_object *desc,
				    struct acpi_device_data *data);

static bool acpi_enumerate_nondev_subnodes(acpi_handle scope,
					   const union acpi_object *desc,
					   struct acpi_device_data *data,
					   struct fwnode_handle *parent);

static bool acpi_properties_format_valid(const union acpi_object *properties);
static bool acpi_is_property_guid(const guid_t *guid);
void nina_init_properties(struct acpi_device *adev);
#endif
