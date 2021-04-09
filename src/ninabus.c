#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>

#include <linux/acpi.h>

#include "ninabus.h"

#define NINA_BUTTON_CLASS "nina_button"

#define NINA_BUTTON_SUBCLASS_POWER  "power"
#define NINA_BUTTON_DEVICE_NAME_POWER "Power Button"

#define NINA_BUTTON_TYPE_POWER 0x01
#define NINA_BUTTON_NOTIFY_STATUS 0x80

#define ACPI_BUTTON_HID_POWER  "PNP0C0C"

static struct proc_dir_entry *nina_button_dir;

struct acpi_device *nina_root;
struct proc_dir_entry *nina_root_dir;


static u32 acpi_device_fixed_event(void *data);
static void acpi_device_notify(acpi_handle handle, u32 event, void *data);

static void acpi_device_remove_notify_handler(struct acpi_device *device)
{
        if (device->device_type == ACPI_BUS_TYPE_POWER_BUTTON)
                acpi_remove_fixed_event_handler(ACPI_EVENT_POWER_BUTTON,
                                                acpi_device_fixed_event);
        else if (device->device_type == ACPI_BUS_TYPE_SLEEP_BUTTON)
                acpi_remove_fixed_event_handler(ACPI_EVENT_SLEEP_BUTTON,
                                                acpi_device_fixed_event);
        else
                acpi_remove_notify_handler(device->handle, ACPI_DEVICE_NOTIFY,
                                           acpi_device_notify);
}

static void acpi_device_notify(acpi_handle handle, u32 event, void *data)
{
        struct acpi_device *device = data;

        device->driver->ops.notify(device, event);
}

static void acpi_device_notify_fixed(void *data)
{
        struct acpi_device *device = data;

        /* Fixed hardware devices have no handles */
        acpi_device_notify(NULL, ACPI_FIXED_HARDWARE_EVENT, device);
}

static u32 acpi_device_fixed_event(void *data)
{
        acpi_os_execute(OSL_NOTIFY_HANDLER, acpi_device_notify_fixed, data);
        return ACPI_INTERRUPT_HANDLED;
}

static int acpi_device_install_notify_handler(struct acpi_device *device)
{
        acpi_status status;

        if (device->device_type == ACPI_BUS_TYPE_POWER_BUTTON)
                status =
                    acpi_install_fixed_event_handler(ACPI_EVENT_POWER_BUTTON,
                                                     acpi_device_fixed_event,
                                                     device);
        else if (device->device_type == ACPI_BUS_TYPE_SLEEP_BUTTON)
                status =
                    acpi_install_fixed_event_handler(ACPI_EVENT_SLEEP_BUTTON,
                                                     acpi_device_fixed_event,
                                                     device);
        else
                status = acpi_install_notify_handler(device->handle,
                                                     ACPI_DEVICE_NOTIFY,
                                                     acpi_device_notify,
                                                     device);

        if (ACPI_FAILURE(status))
                return -EINVAL;
        return 0;
}

static int nina_bus_match(struct device *dev, struct device_driver *drv)
{
        struct acpi_device *acpi_dev = to_acpi_device(dev);
        struct acpi_driver *acpi_drv = to_acpi_driver(dev->driver);

        return acpi_dev->flags.match_driver
                && !acpi_match_device_ids(acpi_dev, acpi_drv->ids);
}

static int nina_device_probe(struct device *dev)
{
        struct acpi_device *acpi_dev = to_acpi_device(dev);
        struct acpi_driver *acpi_drv = to_acpi_driver(dev->driver);
        int ret;
        
        if (acpi_dev->handler && !acpi_is_pnp_device(acpi_dev))
                return -EINVAL;

        if (!acpi_drv->ops.add)
                return -ENOSYS;

        ret = acpi_drv->ops.add(acpi_dev);
        if (ret)
                return ret;

        acpi_dev->driver = acpi_drv;

        if (acpi_drv->ops.notify) {
                ret = acpi_device_install_notify_handler(acpi_dev);
                if (ret) {
                        if (acpi_drv->ops.remove)
                                acpi_drv->ops.remove(acpi_dev);

                        acpi_dev->driver = NULL;
                        acpi_dev->driver_data = NULL;
                        return ret;
                }
        }

        get_device(dev);
        return 0;
}

static int nina_device_remove(struct device *dev)
{
        struct acpi_device *acpi_dev = to_acpi_device(dev);
        struct acpi_driver *acpi_drv = acpi_dev->driver;

        if (acpi_drv) {
                if (acpi_drv->ops.notify)
                        acpi_device_remove_notify_handler(acpi_dev);
                if (acpi_drv->ops.remove)
                        acpi_drv->ops.remove(acpi_dev);
        }
        acpi_dev->driver = NULL;
        acpi_dev->driver_data = NULL;

        put_device(dev);
        return 0;

}

struct bus_type nina_bus_type = {
        .name   = "nina",
        .match  = nina_bus_match,
        .probe  = nina_device_probe,
        .remove = nina_device_remove,
        //.uevent =

};

int nina_bus_register_driver(struct acpi_driver *driver)
{
        int ret;

        driver->drv.name = driver->name;
        driver->drv.bus = &nina_bus_type;
        driver->drv.owner = driver->owner;

        ret = driver_register(&driver->drv);
        return ret;
}

EXPORT_SYMBOL(nina_bus_register_driver);

int __init nina_bus_init(void)
{
	int result;
	printk("init nina bus\n");

	result = bus_register(&nina_bus_type);
      //  if (!result)
        //        return 0;

	return 0;
}
