#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/acpi.h>
#include <linux/device.h>

#include "ninabus.h"

static int nina_match(struct device *dev, struct device_driver *driver);
static  int nina_probe(struct device *dev);
struct bus_type nina_bus_type = {
	.name = "nina",
	.match = nina_match,
	.probe = nina_probe,
//	.remove = nina_remove

};

EXPORT_SYMBOL_GPL(nina_bus_type);

int nina_register_driver(struct nina_driver *drv)
{
	int ret;
	
	drv->driver.bus = &nina_bus_type;
	ret = driver_register(&drv->driver);

	if (ret)
		return ret;

	return 0;
}

EXPORT_SYMBOL(nina_register_driver);


static int nina_remove(struct device *dev)
{
//	struct nina_device *nina_dev = to_nina_device(dev);
//	struct nina_driver *nina_drv = to_nina_driver(dev->driver);

//	nina_drv->remove(nina_dev);

	return 0;	
}
static int nina_match(struct device *dev, struct device_driver *driver)
{
	struct nina_device *nina_dev = to_nina_device(dev);
	
	if(!strcmp(nina_dev->type,"nina"))
		return 1;

	return 0;
}


static  int nina_probe(struct device *dev)
{
	struct nina_device *nina_dev = to_nina_device(dev);
	struct nina_driver *nina_drv = to_nina_driver(dev->driver);

	return nina_drv->probe(nina_dev);
}

int nina_bus_init(void)
{
  int ret;	
  printk("init");

  ret = bus_register(&nina_bus_type);
 
  if (ret < 0){
	  pr_err("NINA Unable to register bus\n");
	  return ret;
  }

  return 0;
}
