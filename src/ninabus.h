#ifndef _NINABUS_H_
#define _NINABUS_H_

#include <linux/device.h>

struct  nina_device {
	const char *type;
	int version;
	struct device dev;
};

#define to_nina_device(drv) container_of(dev, struct nina_device,dev)

struct nina_driver {
	const char *type;
	int (*probe) (struct nina_device *dev);
//      void (*remove)(struct nina_device *dev);
        struct device_driver driver;	
};

#define to_nina_driver(drv) container_of(drv, struct nina_driver, driver)
int nina_register_driver(struct nina_driver *drv);

int nina_bus_init(void);
#endif
