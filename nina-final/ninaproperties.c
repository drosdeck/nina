#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>
#include <linux/list.h>
#include <linux/fwnode.h>

#include "ninaproperties.h"



static const guid_t prp_guids[] = {
	/* ACPI _DSD device properties GUID: daffd814-6eba-4d8c-8a91-bc9bbf4aa301 */
	GUID_INIT(0xdaffd814, 0x6eba, 0x4d8c,
		  0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01),
	/* Hotplug in D3 GUID: 6211e2c0-58a3-4af3-90e1-927a4e0c55a4 */
	GUID_INIT(0x6211e2c0, 0x58a3, 0x4af3,
		  0x90, 0xe1, 0x92, 0x7a, 0x4e, 0x0c, 0x55, 0xa4),
	/* External facing port GUID: efcc06cc-73ac-4bc3-bff0-76143807c389 */
	GUID_INIT(0xefcc06cc, 0x73ac, 0x4bc3,
		  0xbf, 0xf0, 0x76, 0x14, 0x38, 0x07, 0xc3, 0x89),
	/* Thunderbolt GUID for IMR_VALID: c44d002f-69f9-4e7d-a904-a7baabdf43f7 */
	GUID_INIT(0xc44d002f, 0x69f9, 0x4e7d,
		  0xa9, 0x04, 0xa7, 0xba, 0xab, 0xdf, 0x43, 0xf7),
	/* Thunderbolt GUID for WAKE_SUPPORTED: 6c501103-c189-4296-ba72-9bf5a26ebe5d */
	GUID_INIT(0x6c501103, 0xc189, 0x4296,
		  0xba, 0x72, 0x9b, 0xf5, 0xa2, 0x6e, 0xbe, 0x5d),
	/* Storage device needs D3 GUID: 5025030f-842f-4ab4-a561-99a5189762d0 */
	GUID_INIT(0x5025030f, 0x842f, 0x4ab4,
		  0xa5, 0x61, 0x99, 0xa5, 0x18, 0x97, 0x62, 0xd0),
};

static const guid_t ads_guid =
	GUID_INIT(0xdbb8e3e6, 0x5886, 0x4ba6,
		  0x87, 0x95, 0x13, 0x19, 0xf5, 0x2a, 0x96, 0x6b);


static bool acpi_nondev_subnode_extract(const union acpi_object *desc,
					acpi_handle handle,
					const union acpi_object *link,
					struct list_head *list,
					struct fwnode_handle *parent)
{
	struct acpi_data_node *dn;
	bool result;

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn)
		return false;

	dn->name = link->package.elements[0].string.pointer;
	fwnode_init(&dn->fwnode, &acpi_data_fwnode_ops);
	dn->parent = parent;
	INIT_LIST_HEAD(&dn->data.properties);
	INIT_LIST_HEAD(&dn->data.subnodes);

	result = acpi_extract_properties(desc, &dn->data);

	if (handle) {
		acpi_handle scope;
		acpi_status status;

		/*
		 * The scope for the subnode object lookup is the one of the
		 * namespace node (device) containing the object that has
		 * returned the package.  That is, it's the scope of that
		 * object's parent.
		 */
		status = acpi_get_parent(handle, &scope);
		if (ACPI_SUCCESS(status)
		    && acpi_enumerate_nondev_subnodes(scope, desc, &dn->data,
						      &dn->fwnode))
			result = true;
	} else if (acpi_enumerate_nondev_subnodes(NULL, desc, &dn->data,
						  &dn->fwnode)) {
		result = true;
	}

	if (result) {
		dn->handle = handle;
		dn->data.pointer = desc;
		list_add_tail(&dn->sibling, list);
		return true;
	}

	kfree(dn);
	acpi_handle_debug(handle, "Invalid properties/subnodes data, skipping\n");
	return false;
}

static bool acpi_nondev_subnode_data_ok(acpi_handle handle,
					const union acpi_object *link,
					struct list_head *list,
					struct fwnode_handle *parent)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	acpi_status status;

	status = acpi_evaluate_object_typed(handle, NULL, NULL, &buf,
					    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status))
		return false;

	if (acpi_nondev_subnode_extract(buf.pointer, handle, link, list,
					parent))
		return true;

	ACPI_FREE(buf.pointer);
	return false;
}

static bool acpi_nondev_subnode_ok(acpi_handle scope,
				   const union acpi_object *link,
				   struct list_head *list,
				   struct fwnode_handle *parent)
{
	acpi_handle handle;
	acpi_status status;

	if (!scope)
		return false;

	status = acpi_get_handle(scope, link->package.elements[1].string.pointer,
				 &handle);
	if (ACPI_FAILURE(status))
		return false;

	return acpi_nondev_subnode_data_ok(handle, link, list, parent);
}


static int acpi_add_nondev_subnodes(acpi_handle scope,
				    const union acpi_object *links,
				    struct list_head *list,
				    struct fwnode_handle *parent)
{
	bool ret = false;
	int i;

	for (i = 0; i < links->package.count; i++) {
		const union acpi_object *link, *desc;
		acpi_handle handle;
		bool result;

		link = &links->package.elements[i];
		/* Only two elements allowed. */
		if (link->package.count != 2)
			continue;

		/* The first one must be a string. */
		if (link->package.elements[0].type != ACPI_TYPE_STRING)
			continue;

		/* The second one may be a string, a reference or a package. */
		switch (link->package.elements[1].type) {
		case ACPI_TYPE_STRING:
			result = acpi_nondev_subnode_ok(scope, link, list,
							 parent);
			break;
		case ACPI_TYPE_LOCAL_REFERENCE:
			handle = link->package.elements[1].reference.handle;
			result = acpi_nondev_subnode_data_ok(handle, link, list,
							     parent);
			break;
		case ACPI_TYPE_PACKAGE:
			desc = &link->package.elements[1];
			result = acpi_nondev_subnode_extract(desc, NULL, link,
							     list, parent);
			break;
		default:
			result = false;
			break;
		}
		ret = ret || result;
	}

	return ret;
}

static bool acpi_enumerate_nondev_subnodes(acpi_handle scope,
					   const union acpi_object *desc,
					   struct acpi_device_data *data,
					   struct fwnode_handle *parent)
{
	int i;

	/* Look for the ACPI data subnodes GUID. */
	for (i = 0; i < desc->package.count; i += 2) {
		const union acpi_object *guid, *links;

		guid = &desc->package.elements[i];
		links = &desc->package.elements[i + 1];

		/*
		 * The first element must be a GUID and the second one must be
		 * a package.
		 */
		if (guid->type != ACPI_TYPE_BUFFER ||
		    guid->buffer.length != 16 ||
		    links->type != ACPI_TYPE_PACKAGE)
			break;

		if (!guid_equal((guid_t *)guid->buffer.pointer, &ads_guid))
			continue;

		return acpi_add_nondev_subnodes(scope, links, &data->subnodes,
						parent);
	}

	return false;
}

static int acpi_data_get_property(const struct acpi_device_data *data,
				  const char *name, acpi_object_type type,
				  const union acpi_object **obj)
{
	const struct acpi_device_properties *props;

	if (!data || !name)
		return -EINVAL;

	if (!data->pointer || list_empty(&data->properties))
		return -EINVAL;

	list_for_each_entry(props, &data->properties, list) {
		const union acpi_object *properties;
		unsigned int i;

		properties = props->properties;
		for (i = 0; i < properties->package.count; i++) {
			const union acpi_object *propname, *propvalue;
			const union acpi_object *property;

			property = &properties->package.elements[i];

			propname = &property->package.elements[0];
			propvalue = &property->package.elements[1];

			if (!strcmp(name, propname->string.pointer)) {
				if (type != ACPI_TYPE_ANY &&
				    propvalue->type != type)
					return -EPROTO;
				if (obj)
					*obj = propvalue;

				return 0;
			}
		}
	}
	return -EINVAL;
}


static int acpi_data_get_property_array(const struct acpi_device_data *data,
					const char *name,
					acpi_object_type type,
					const union acpi_object **obj)
{
	const union acpi_object *prop;
	int ret, i;

	ret = acpi_data_get_property(data, name, ACPI_TYPE_PACKAGE, &prop);
	if (ret)
		return ret;

	if (type != ACPI_TYPE_ANY) {
		/* Check that all elements are of correct type. */
		for (i = 0; i < prop->package.count; i++)
			if (prop->package.elements[i].type != type)
				return -EPROTO;
	}
	if (obj)
		*obj = prop;

	return 0;
}

static void acpi_init_of_compatible(struct acpi_device *adev)
{
	const union acpi_object *of_compatible;
	int ret;

	ret = acpi_data_get_property_array(&adev->data, "compatible",
					   ACPI_TYPE_STRING, &of_compatible);
	if (ret) {
		ret = acpi_dev_get_property(adev, "compatible",
					    ACPI_TYPE_STRING, &of_compatible);
		if (ret) {
			if (adev->parent
			    && adev->parent->flags.of_compatible_ok)
				goto out;

			return;
		}
	}
	adev->data.of_compatible = of_compatible;

 out:
	adev->flags.of_compatible_ok = 1;
}


static bool acpi_property_value_ok(const union acpi_object *value)
{
	int j;

	/*
	 * The value must be an integer, a string, a reference, or a package
	 * whose every element must be an integer, a string, or a reference.
	 */
	switch (value->type) {
	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_LOCAL_REFERENCE:
		return true;

	case ACPI_TYPE_PACKAGE:
		for (j = 0; j < value->package.count; j++)
			switch (value->package.elements[j].type) {
			case ACPI_TYPE_INTEGER:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_LOCAL_REFERENCE:
				continue;

			default:
				return false;
			}

		return true;
	}
	return false;
}


static bool acpi_properties_format_valid(const union acpi_object *properties)
{
	int i;

	for (i = 0; i < properties->package.count; i++) {
		const union acpi_object *property;

		property = &properties->package.elements[i];
		/*
		 * Only two elements allowed, the first one must be a string and
		 * the second one has to satisfy certain conditions.
		 */
		if (property->package.count != 2
		    || property->package.elements[0].type != ACPI_TYPE_STRING
		    || !acpi_property_value_ok(&property->package.elements[1]))
			return false;
	}
	return true;
}

static bool acpi_is_property_guid(const guid_t *guid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(prp_guids); i++) {
		if (guid_equal(guid, &prp_guids[i]))
			return true;
	}

	return false;
}

static bool acpi_extract_properties(const union acpi_object *desc,
				    struct acpi_device_data *data)
{
	int i;

	if (desc->package.count % 2)
		return false;

	/* Look for the device properties GUID. */
	for (i = 0; i < desc->package.count; i += 2) {
		const union acpi_object *guid, *properties;

		guid = &desc->package.elements[i];
		properties = &desc->package.elements[i + 1];

		/*
		 * The first element must be a GUID and the second one must be
		 * a package.
		 */
		if (guid->type != ACPI_TYPE_BUFFER ||
		    guid->buffer.length != 16 ||
		    properties->type != ACPI_TYPE_PACKAGE)
			break;

		if (!acpi_is_property_guid((guid_t *)guid->buffer.pointer))
			continue;

		/*
		 * We found the matching GUID. Now validate the format of the
		 * package immediately following it.
		 */
		if (!acpi_properties_format_valid(properties))
			continue;

		acpi_data_add_props(data, (const guid_t *)guid->buffer.pointer,
				    properties);
	}

	return !list_empty(&data->properties);
}


void nina_init_properties(struct acpi_device *adev)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	struct acpi_hardware_id *hwid;
	acpi_status status;
	bool acpi_of = false;

	INIT_LIST_HEAD(&adev->data.properties);
	INIT_LIST_HEAD(&adev->data.subnodes);

	if (!adev->handle)
		return;
/*
	 * Check if ACPI_DT_NAMESPACE_HID is present and inthat case we fill in
	 * Device Tree compatible properties for this device.
	 */
	list_for_each_entry(hwid, &adev->pnp.ids, list) {
		if (!strcmp(hwid->id, ACPI_DT_NAMESPACE_HID)) {
			acpi_of = true;
			break;
		}
	}

	status = acpi_evaluate_object_typed(adev->handle, "_DSD", NULL, &buf,
					    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status))
		goto out;

	if (acpi_extract_properties(buf.pointer, &adev->data)) {
		adev->data.pointer = buf.pointer;
		if (acpi_of)
			acpi_init_of_compatible(adev);
	}
	if (acpi_enumerate_nondev_subnodes(adev->handle, buf.pointer,
					&adev->data, acpi_fwnode_handle(adev)))
		adev->data.pointer = buf.pointer;

	if (!adev->data.pointer) {
		acpi_handle_debug(adev->handle, "Invalid _DSD data, skipping\n");
		ACPI_FREE(buf.pointer);
	}

 out:
	if (acpi_of && !adev->flags.of_compatible_ok)
		acpi_handle_info(adev->handle,
			 ACPI_DT_NAMESPACE_HID " requires 'compatible' property\n");

	//if (!adev->data.pointer)
	//	acpi_extract_apple_properties(adev);
}
