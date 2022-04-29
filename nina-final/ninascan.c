#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>
#include <linux/list.h>
#include <linux/fwnode.h>

#include <linux/device.h>
#include "bex.h"

#include "ninabus.h"
#include "ninaproperties.h"
#include "ninapower.h"
extern struct acpi_device *nina_root;
#define ACPI_BUS_CLASS			"system_bus"
#define ACPI_BUS_HID			"LNXSYBUS"
#define ACPI_BUS_DEVICE_NAME		"System Bus"
#define ACPI_IS_ROOT_DEVICE(device)    (!(device)->parent)

//--------------------------

static int bex_match(struct device *dev, struct device_driver *driver);
static int bex_probe(struct device *dev);

 struct bus_type bex_bus_type = {
		.name = "nina",
		.match = bex_match,
		.probe = bex_probe,	

 };


static int bex_match(struct device *dev, struct device_driver *driver){
	struct bex_device *bex_dev = to_bex_device(dev);
	struct bex_driver *bex_drv = to_bex_driver(driver);

	if(!strcmp(bex_dev->type,"nina"))
		return 1;

	return 0;
}


static int bex_probe(struct device *dev)
{
	struct bex_device *bex_dev = to_bex_device(dev);
	struct bex_driver *bex_drv = to_bex_driver(dev->driver);

	return  bex_drv->probe(bex_dev);

}
//---------------------

int nina_device_add(struct acpi_device *adev)
{
	int result;
	printk("nina adicionado\n");
        dev_set_name(&adev->dev, "%s:%02x","teste", result);      
	adev->dev.bus = &bex_bus_type;
        
	result = device_add(&adev->dev);
	if (result) {
		dev_err(&adev->dev, "Error registering device\n");
         }
return 0;
}

static void acpi_bus_get_flags(struct acpi_device *device)
{
	/* Presence of _STA indicates 'dynamic_status' */
	if (acpi_has_method(device->handle, "_STA"))
		device->flags.dynamic_status = 1;

	/* Presence of _RMV indicates 'removable' */
	if (acpi_has_method(device->handle, "_RMV"))
		device->flags.removable = 1;

	/* Presence of _EJD|_EJ0 indicates 'ejectable' */
	if (acpi_has_method(device->handle, "_EJD") ||
	    acpi_has_method(device->handle, "_EJ0"))
		device->flags.ejectable = 1;
}

void nina_free_pnp_ids(struct acpi_device_pnp *pnp)
{
	struct acpi_hardware_id *id, *tmp;

	list_for_each_entry_safe(id, tmp, &pnp->ids, list) {
		kfree_const(id->id);
		kfree(id);
	}
	kfree(pnp->unique_id);
}

void nina_device_add_finalize(struct acpi_device *device)
{
	dev_set_uevent_suppress(&device->dev, false);
	kobject_uevent(&device->dev.kobj, KOBJ_ADD);
}

static void acpi_bus_init_power_state(struct acpi_device *device, int state)
{
	struct acpi_device_power_state *ps = &device->power.states[state];
	char pathname[5] = { '_', 'P', 'R', '0' + state, '\0' };
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;

	INIT_LIST_HEAD(&ps->resources);

	/* Evaluate "_PRx" to get referenced power resources */
	status = acpi_evaluate_object(device->handle, pathname, NULL, &buffer);
	if (ACPI_SUCCESS(status)) {
		union acpi_object *package = buffer.pointer;

		if (buffer.length && package
		    && package->type == ACPI_TYPE_PACKAGE
		    && package->package.count)
			nina_extract_power_resources(package, 0, &ps->resources);

		ACPI_FREE(buffer.pointer);
	}

	/* Evaluate "_PSx" to see if we can do explicit sets */
	pathname[2] = 'S';
	if (acpi_has_method(device->handle, pathname))
		ps->flags.explicit_set = 1;

	/* State is valid if there are means to put the device into it. */
	if (!list_empty(&ps->resources) || ps->flags.explicit_set)
		ps->flags.valid = 1;

	ps->power = -1;		/* Unknown - driver assigned */
	ps->latency = -1;	/* Unknown - driver assigned */
}

static void acpi_bus_get_power_flags(struct acpi_device *device)
{
	u32 i;

	/* Presence of _PS0|_PR0 indicates 'power manageable' */
	if (!acpi_has_method(device->handle, "_PS0") &&
	    !acpi_has_method(device->handle, "_PR0"))
		return;

	device->flags.power_manageable = 1;

	/*
	 * Power Management Flags
	 */
	if (acpi_has_method(device->handle, "_PSC"))
		device->power.flags.explicit_get = 1;

	if (acpi_has_method(device->handle, "_IRC"))
		device->power.flags.inrush_current = 1;

	if (acpi_has_method(device->handle, "_DSW"))
		device->power.flags.dsw_present = 1;

	/*
	 * Enumerate supported power management states
	 */
	for (i = ACPI_STATE_D0; i <= ACPI_STATE_D3_HOT; i++)
		acpi_bus_init_power_state(device, i);

	INIT_LIST_HEAD(&device->power.states[ACPI_STATE_D3_COLD].resources);

	/* Set the defaults for D0 and D3hot (always supported). */
	device->power.states[ACPI_STATE_D0].flags.valid = 1;
	device->power.states[ACPI_STATE_D0].power = 100;
	device->power.states[ACPI_STATE_D3_HOT].flags.valid = 1;

	/*
	 * Use power resources only if the D0 list of them is populated, because
	 * some platforms may provide _PR3 only to indicate D3cold support and
	 * in those cases the power resources list returned by it may be bogus.
	 */
	if (!list_empty(&device->power.states[ACPI_STATE_D0].resources)) {
		device->power.flags.power_resources = 1;
		/*
		 * D3cold is supported if the D3hot list of power resources is
		 * not empty.
		 */
		if (!list_empty(&device->power.states[ACPI_STATE_D3_HOT].resources))
			device->power.states[ACPI_STATE_D3_COLD].flags.valid = 1;
	}

	if (acpi_bus_init_power(device))
		device->flags.power_manageable = 0;
}

static void acpi_scan_init_status(struct acpi_device *adev)
{
	if (acpi_bus_get_status(adev))
		acpi_set_device_status(adev, 0);
}

static void acpi_init_coherency(struct acpi_device *adev)
{
	unsigned long long cca = 0;
	acpi_status status;
	struct acpi_device *parent = adev->parent;

	if (parent && parent->flags.cca_seen) {
		/*
		 * From ACPI spec, OSPM will ignore _CCA if an ancestor
		 * already saw one.
		 */
		adev->flags.cca_seen = 1;
		cca = parent->flags.coherent_dma;
	} else {
		status = acpi_evaluate_integer(adev->handle, "_CCA",
					       NULL, &cca);
		if (ACPI_SUCCESS(status))
			adev->flags.cca_seen = 1;
		else if (!IS_ENABLED(CONFIG_ACPI_CCA_REQUIRED))
			/*
			 * If architecture does not specify that _CCA is
			 * required for DMA-able devices (e.g. x86),
			 * we default to _CCA=1.
			 */
			cca = 1;
		else
			acpi_handle_debug(adev->handle,
					  "ACPI device is missing _CCA.\n");
	}

	adev->flags.coherent_dma = cca;
}

static int acpi_check_serial_bus_slave(struct acpi_resource *ares, void *data)
{
	bool *is_serial_bus_slave_p = data;

	if (ares->type != ACPI_RESOURCE_TYPE_SERIAL_BUS)
		return 1;

	*is_serial_bus_slave_p = true;

	 /* no need to do more checking */
	return -1;
}

static bool acpi_is_indirect_io_slave(struct acpi_device *device)
{
	struct acpi_device *parent = device->parent;
	static const struct acpi_device_id indirect_io_hosts[] = {
		{"HISI0191", 0},
		{}
	};

	return parent && !acpi_match_device_ids(parent, indirect_io_hosts);
}


static bool acpi_device_enumeration_by_parent(struct acpi_device *device)
{
	struct list_head resource_list;
	bool is_serial_bus_slave = false;
	static const struct acpi_device_id ignore_serial_bus_ids[] = {
	/*
	 * These devices have multiple I2cSerialBus resources and an i2c-client
	 * must be instantiated for each, each with its own i2c_device_id.
	 * Normally we only instantiate an i2c-client for the first resource,
	 * using the ACPI HID as id. These special cases are handled by the
	 * drivers/platform/x86/i2c-multi-instantiate.c driver, which knows
	 * which i2c_device_id to use for each resource.
	 */
		{"BSG1160", },
		{"BSG2150", },
		{"INT33FE", },
		{"INT3515", },
	/*
	 * HIDs of device with an UartSerialBusV2 resource for which userspace
	 * expects a regular tty cdev to be created (instead of the in kernel
	 * serdev) and which have a kernel driver which expects a platform_dev
	 * such as the rfkill-gpio driver.
	 */
		{"BCM4752", },
		{"LNV4752", },
		{}
	};

	if (acpi_is_indirect_io_slave(device))
		return true;

	/* Macs use device properties in lieu of _CRS resources */
	//if (x86_apple_machine &&
	//    (fwnode_property_present(&device->fwnode, "spiSclkPeriod") ||
	//     fwnode_property_present(&device->fwnode, "i2cAddress") ||
	//     fwnode_property_present(&device->fwnode, "baud")))
        //		return true;

	if (!acpi_match_device_ids(device, ignore_serial_bus_ids))
		return false;

	INIT_LIST_HEAD(&resource_list);
	acpi_dev_get_resources(device, &resource_list,
			       acpi_check_serial_bus_slave,
			       &is_serial_bus_slave);
	acpi_dev_free_resource_list(&resource_list);

	return is_serial_bus_slave;
}

static bool acpi_object_is_system_bus(acpi_handle handle)
{
	acpi_handle tmp;

	if (ACPI_SUCCESS(acpi_get_handle(NULL, "\\_SB", &tmp)) &&
	    tmp == handle)
		return true;
	if (ACPI_SUCCESS(acpi_get_handle(NULL, "\\_TZ", &tmp)) &&
	    tmp == handle)
		return true;

	return false;
}

static void acpi_add_id(struct acpi_device_pnp *pnp, const char *dev_id)
{
	struct acpi_hardware_id *id;

	id = kmalloc(sizeof(*id), GFP_KERNEL);
	if (!id)
		return;

	id->id = kstrdup_const(dev_id, GFP_KERNEL);
	if (!id->id) {
		kfree(id);
		return;
	}

	list_add_tail(&id->list, &pnp->ids);
	pnp->type.hardware_id = 1;
}

/*
 * Old IBM workstations have a DSDT bug wherein the SMBus object
 * lacks the SMBUS01 HID and the methods do not have the necessary "_"
 * prefix.  Work around this.
 */
//static bool acpi_ibm_smbus_match(acpi_handle handle)
//{
//	char node_name[ACPI_PATH_SEGMENT_LENGTH];
//	struct acpi_buffer path = { sizeof(node_name), node_name };
//
//	if (!dmi_name_in_vendors("IBM"))
//		return false;
//
//	/* Look for SMBS object */
//	if (ACPI_FAILURE(acpi_get_name(handle, ACPI_SINGLE_NAME, &path)) ||
//	    strcmp("SMBS", path.pointer))
//		return false;
//
//	/* Does it have the necessary (but misnamed) methods? */
//	if (acpi_has_method(handle, "SBI") &&
//	    acpi_has_method(handle, "SBR") &&
//	    acpi_has_method(handle, "SBW"))
//		return true;
//
//	return false;
//}

static void acpi_set_pnp_ids(acpi_handle handle, struct acpi_device_pnp *pnp,
			     int device_type)
{
	struct acpi_device_info *info = NULL;
	struct acpi_pnp_device_id_list *cid_list;
	int i;

	switch (device_type) {
	case ACPI_BUS_TYPE_DEVICE:
		if (handle == ACPI_ROOT_OBJECT) {
			acpi_add_id(pnp, ACPI_SYSTEM_HID);
			break;
		}

		acpi_get_object_info(handle, &info);
		if (!info) {
			pr_err("%s: Error reading device info\n", __func__);
			return;
		}

		if (info->valid & ACPI_VALID_HID) {
			acpi_add_id(pnp, info->hardware_id.string);
			pnp->type.platform_id = 1;
		}
		if (info->valid & ACPI_VALID_CID) {
			cid_list = &info->compatible_id_list;
			for (i = 0; i < cid_list->count; i++)
				acpi_add_id(pnp, cid_list->ids[i].string);
		}
		if (info->valid & ACPI_VALID_ADR) {
			pnp->bus_address = info->address;
			pnp->type.bus_address = 1;
		}
		if (info->valid & ACPI_VALID_UID)
			pnp->unique_id = kstrdup(info->unique_id.string,
							GFP_KERNEL);
		if (info->valid & ACPI_VALID_CLS)
			acpi_add_id(pnp, info->class_code.string);

		kfree(info);

		/*
		 * Some devices don't reliably have _HIDs & _CIDs, so add
		 * synthetic HIDs to make sure drivers can find them.
		 */
		if (acpi_is_video_device(handle))
			acpi_add_id(pnp, ACPI_VIDEO_HID);
		else if (acpi_bay_match(handle))
			acpi_add_id(pnp, ACPI_BAY_HID);
		else if (acpi_dock_match(handle))
			acpi_add_id(pnp, ACPI_DOCK_HID);
		else if (list_empty(&pnp->ids) &&
			 acpi_object_is_system_bus(handle)) {
			/* \_SB, \_TZ, LNXSYBUS */
			acpi_add_id(pnp, ACPI_BUS_HID);
			strcpy(pnp->device_name, ACPI_BUS_DEVICE_NAME);
			strcpy(pnp->device_class, ACPI_BUS_CLASS);
		}

		break;
	case ACPI_BUS_TYPE_POWER:
		acpi_add_id(pnp, ACPI_POWER_HID);
		break;
	case ACPI_BUS_TYPE_PROCESSOR:
		acpi_add_id(pnp, ACPI_PROCESSOR_OBJECT_HID);
		break;
	case ACPI_BUS_TYPE_THERMAL:
		acpi_add_id(pnp, ACPI_THERMAL_HID);
		break;
	case ACPI_BUS_TYPE_POWER_BUTTON:
		acpi_add_id(pnp, ACPI_BUTTON_HID_POWERF);
		break;
	case ACPI_BUS_TYPE_SLEEP_BUTTON:
		acpi_add_id(pnp, ACPI_BUTTON_HID_SLEEPF);
		break;
	case ACPI_BUS_TYPE_ECDT_EC:
		acpi_add_id(pnp, ACPI_ECDT_HID);
		break;
	}
}

static void acpi_device_get_busid(struct acpi_device *device)
{
	char bus_id[5] = { '?', 0 };
	struct acpi_buffer buffer = { sizeof(bus_id), bus_id };
	int i = 0;

	/*
	 * Bus ID
	 * ------
	 * The device's Bus ID is simply the object name.
	 * TBD: Shouldn't this value be unique (within the ACPI namespace)?
	 */
	if (ACPI_IS_ROOT_DEVICE(device)) {
		strcpy(device->pnp.bus_id, "ACPI");
		return;
	}

	switch (device->device_type) {
	case ACPI_BUS_TYPE_POWER_BUTTON:
		strcpy(device->pnp.bus_id, "PWRF");
		break;
	case ACPI_BUS_TYPE_SLEEP_BUTTON:
		strcpy(device->pnp.bus_id, "SLPF");
		break;
	case ACPI_BUS_TYPE_ECDT_EC:
		strcpy(device->pnp.bus_id, "ECDT");
		break;
	default:
		acpi_get_name(device->handle, ACPI_SINGLE_NAME, &buffer);
		/* Clean up trailing underscores (if any) */
		for (i = 3; i > 1; i--) {
			if (bus_id[i] == '_')
				bus_id[i] = '\0';
			else
				break;
		}
		strcpy(device->pnp.bus_id, bus_id);
		break;
	}
}

static struct acpi_device *nina_bus_get_parent(acpi_handle handle)
{
	struct acpi_device *device = NULL;
	acpi_status status;

	/*
	 * Fixed hardware devices do not appear in the namespace and do not
	 * have handles, but we fabricate acpi_devices for them, so we have
	 * to deal with them specially.
	 */
	if (!handle)
		return nina_root;

	do {
		status = acpi_get_parent(handle, &handle);
		if (ACPI_FAILURE(status))
			return status == AE_NULL_ENTRY ? NULL : nina_root;
	} while (acpi_bus_get_device(handle, &device));
	return device;
}


void nina_init_device_object(struct acpi_device *device, acpi_handle handle,
			     int type)
{
	INIT_LIST_HEAD(&device->pnp.ids);
	device->device_type = type;
	device->handle = handle;
        device->parent = nina_bus_get_parent(handle);
	fwnode_init(&device->fwnode, &acpi_device_fwnode_ops);
	acpi_set_device_status(device, ACPI_STA_DEFAULT);
	acpi_device_get_busid(device);
	acpi_set_pnp_ids(handle, &device->pnp, type);
	nina_init_properties(device);
	acpi_bus_get_flags(device);
	device->flags.match_driver = false;
	device->flags.initialized = true;
	device->flags.enumeration_by_parent =
		acpi_device_enumeration_by_parent(device);
	acpi_device_clear_enumerated(device);
	device_initialize(&device->dev);
	dev_set_uevent_suppress(&device->dev, true);
	acpi_init_coherency(device);
}

static int nina_add_single_object(struct acpi_device **child,
				  acpi_handle handle, int type, bool dep_init)
{
	struct acpi_device *device;
	bool release_dep_lock = false;
	int result;

	device = kzalloc(sizeof(struct acpi_device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;
        
    	nina_init_device_object(device, handle, type);
	if (type == ACPI_BUS_TYPE_DEVICE || type == ACPI_BUS_TYPE_PROCESSOR) {
		printk("NINA entrou no if tipe =%d",type);	
		if (dep_init) {
			//falta um pedaco
		  printk("NINA entrou no dep_init\n");
		}
		acpi_scan_init_status(device);
               //aqui
	       acpi_bus_get_power_flags(device);
             //adiciona  	
             nina_device_add(device);
	}
	return 0;
}

static acpi_status nina_bus_check_add(acpi_handle handle, bool check_dep,
				      struct acpi_device **adev_p)
{
	struct acpi_device *device = NULL;
	acpi_object_type acpi_type;
	int type;

	acpi_bus_get_device(handle, &device);
	
	if (device){
	   printk("NINA ERRO de device");
	
	return AE_OK;
	}

	if (ACPI_FAILURE(acpi_get_type(handle, &acpi_type)))
		return AE_OK;

	switch (acpi_type) {

		case ACPI_TYPE_ANY:	/* for ACPI_ROOT_OBJECT */
			type = ACPI_BUS_TYPE_DEVICE;
			break;
		default:
		return AE_OK;	

	}
	nina_add_single_object(&device, handle, type, !check_dep);
         printk("nina vallor do type %u",acpi_type);  

	return AE_OK;
	
}

int nina_bus_scan(acpi_handle handle)
{
	struct acpi_device *device = NULL;	
	
	printk("ini");	
	nina_bus_check_add(handle, true, &device);	
	return 0;
}

void __init nina_scan_init(void)
{
	int ret;
  
   printk("nina init scan\n");
  //----------------------
  ret = bus_register(&bex_bus_type);
  if (ret < 0)
	  pr_err("NINA  erro na criacao do bus");

  //-----------------------
  nina_bus_scan(ACPI_ROOT_OBJECT);
}