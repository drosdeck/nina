#ifndef _BEX_H_
#define _BEX_H_

#include <linux/device.h>

struct bex_device {
	const char *type;
	int version;
	struct device dev;
};

#define to_bex_device(drv) container_of(dev,struct bex_device,dev)
struct bex_driver {
	const char *type;
	int (*probe) (struct bex_device *dev);
	struct device_driver driver;
};


#define to_bex_driver(drv) container_of(drv,struct bex_driver,driver)

#endif
