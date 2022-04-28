#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>

#include "ninasysfs.h"

struct kobject *nina_kobj;
EXPORT_SYMBOL_GPL(nina_kobj);

static int __init nina_init(void)
{
	if( !acpi_disabled){
		printk("Nina no work with acpi enabled\n");
		return -ENODEV;
	}

	nina_kobj = kobject_create_and_add("nina", firmware_kobj);
	if (!nina_kobj) {
		printk(KERN_WARNING "%s: kset create error\n", __func__);
		nina_kobj = NULL;
	}
	nina_sysfs_init();
	printk("nina init\n");
	
return 0;
}

subsys_initcall(nina_init);
