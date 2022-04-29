#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>
#include <linux/list.h>
#include <linux/fwnode.h>
#include <linux/sysfs.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>

#include "ninapower.h"
#include "ninascan.h"

#define ACPI_POWER_CLASS		"power_resource"
#define ACPI_POWER_DEVICE_NAME		"Power Resource"
#define ACPI_POWER_RESOURCE_STATE_OFF	0x00
#define ACPI_POWER_RESOURCE_STATE_ON	0x01
#define ACPI_POWER_RESOURCE_STATE_UNKNOWN 0xFF

struct acpi_power_dependent_device {
	struct device *dev;
	struct list_head node;
};

struct acpi_power_resource {
	struct acpi_device device;
	struct list_head list_node;
	u32 system_level;
	u32 order;
	unsigned int ref_count;
	u8 state;
	struct mutex resource_lock;
	struct list_head dependents;
};

struct acpi_power_resource_entry {
	struct list_head node;
	struct acpi_power_resource *resource;
};

static LIST_HEAD(acpi_power_resource_list);
static DEFINE_MUTEX(power_resource_list_lock);

static inline
struct acpi_power_resource *to_power_resource(struct acpi_device *device)
{
	return container_of(device, struct acpi_power_resource, device);
}

void nina_power_resources_list_free(struct list_head *list)
{
	struct acpi_power_resource_entry *entry, *e;

	list_for_each_entry_safe(entry, e, list, node) {
		list_del(&entry->node);
		kfree(entry);
	}
}


//static void acpi_power_sysfs_remove(struct acpi_device *device)
//{
//	device_remove_file(&device->dev, &dev_attr_resource_in_use);
//}


static struct acpi_power_resource *acpi_power_get_context(acpi_handle handle)
{
	struct acpi_device *device;

	if (acpi_bus_get_device(handle, &device))
		return NULL;

	return to_power_resource(device);
}

static int acpi_power_resources_list_add(acpi_handle handle,
					 struct list_head *list)
{
	struct acpi_power_resource *resource = acpi_power_get_context(handle);
	struct acpi_power_resource_entry *entry;

	if (!resource || !list)
		return -EINVAL;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->resource = resource;
	if (!list_empty(list)) {
		struct acpi_power_resource_entry *e;

		list_for_each_entry(e, list, node)
			if (e->resource->order > resource->order) {
				list_add_tail(&entry->node, &e->node);
				return 0;
			}
	}
	list_add_tail(&entry->node, list);
	return 0;
}

static void acpi_power_add_resource_to_list(struct acpi_power_resource *resource)
{
	mutex_lock(&power_resource_list_lock);

	if (!list_empty(&acpi_power_resource_list)) {
		struct acpi_power_resource *r;

		list_for_each_entry(r, &acpi_power_resource_list, list_node)
			if (r->order > resource->order) {
				list_add_tail(&resource->list_node, &r->list_node);
				goto out;
			}
	}
	list_add_tail(&resource->list_node, &acpi_power_resource_list);

 out:
	mutex_unlock(&power_resource_list_lock);
}


static ssize_t resource_in_use_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct acpi_power_resource *resource;

	resource = to_power_resource(to_acpi_device(dev));
	return sprintf(buf, "%u\n", !!resource->ref_count);
}

static DEVICE_ATTR_RO(resource_in_use);
//aqui

static void acpi_power_sysfs_remove(struct acpi_device *device)
{
	device_remove_file(&device->dev, &dev_attr_resource_in_use);
}



static void acpi_release_power_resource(struct device *dev)
{
	struct acpi_device *device = to_acpi_device(dev);
	struct acpi_power_resource *resource;

	resource = container_of(device, struct acpi_power_resource, device);

	mutex_lock(&power_resource_list_lock);
	list_del(&resource->list_node);
	mutex_unlock(&power_resource_list_lock);

	nina_free_pnp_ids(&device->pnp);
	kfree(resource);
}

struct acpi_device *nina_add_power_resource(acpi_handle handle)
{
	struct acpi_power_resource *resource;
	struct acpi_device *device = NULL;
	union acpi_object acpi_object;
	struct acpi_buffer buffer = { sizeof(acpi_object), &acpi_object };
	acpi_status status;
	int result;
        printk("NINA entrou power\n");
	acpi_bus_get_device(handle, &device);
	if (device)
		return device;

	resource = kzalloc(sizeof(*resource), GFP_KERNEL);
	if (!resource)
		return NULL;

	device = &resource->device;
	nina_init_device_object(device, handle, ACPI_BUS_TYPE_POWER);
	mutex_init(&resource->resource_lock);
	INIT_LIST_HEAD(&resource->list_node);
	INIT_LIST_HEAD(&resource->dependents);
	strcpy(acpi_device_name(device), ACPI_POWER_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_POWER_CLASS);
	device->power.state = ACPI_STATE_UNKNOWN;

	/* Evaluate the object to get the system level and resource order. */
	status = acpi_evaluate_object(handle, NULL, NULL, &buffer);
	if (ACPI_FAILURE(status))
		goto err;

	resource->system_level = acpi_object.power_resource.system_level;
	resource->order = acpi_object.power_resource.resource_order;
	resource->state = ACPI_POWER_RESOURCE_STATE_UNKNOWN;

	pr_info("%s [%s]\n", acpi_device_name(device), acpi_device_bid(device));

	device->flags.match_driver = true;
	nina_device_add(device);
//	result = acpi_device_add(device, acpi_release_power_resource);
//	if (result)
//		goto err;

//	if (!device_create_file(&device->dev, &dev_attr_resource_in_use))
//		device->remove = acpi_power_sysfs_remove;

//	acpi_power_add_resource_to_list(resource);
//	acpi_device_add_finalize(device);
	return device;

 err:
	acpi_release_power_resource(&device->dev);
	return NULL;
}


static bool acpi_power_resource_is_dup(union acpi_object *package,
				       unsigned int start, unsigned int i)
{
	acpi_handle rhandle, dup;
	unsigned int j;

	/* The caller is expected to check the package element types */
	rhandle = package->package.elements[i].reference.handle;
	for (j = start; j < i; j++) {
		dup = package->package.elements[j].reference.handle;
		if (dup == rhandle)
			return true;
	}

	return false;
}

int nina_extract_power_resources(union acpi_object *package, unsigned int start,
				 struct list_head *list)
{
	unsigned int i;
	int err = 0;

	for (i = start; i < package->package.count; i++) {
		union acpi_object *element = &package->package.elements[i];
		struct acpi_device *rdev;
		acpi_handle rhandle;

		if (element->type != ACPI_TYPE_LOCAL_REFERENCE) {
			err = -ENODATA;
			break;
		}
		rhandle = element->reference.handle;
		if (!rhandle) {
			err = -ENODEV;
			break;
		}

		/* Some ACPI tables contain duplicate power resource references */
		if (acpi_power_resource_is_dup(package, start, i))
			continue;

		rdev = nina_add_power_resource(rhandle);
		if (!rdev) {
			err = -ENODEV;
			break;
		}
		err = acpi_power_resources_list_add(rhandle, list);
		if (err)
			break;
	}
	if (err)
		nina_power_resources_list_free(list);

	return err;
}

