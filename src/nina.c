#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>

#include "ninasysfs.h"
#include "ninascan.h"
#include "ninaosl.h"

struct kobject *nina_kobj;
EXPORT_SYMBOL_GPL(nina_kobj);

static int __init nina_init(void)
{
	acpi_status status;

	if( !acpi_disabled){
		printk("Nina no work with acpi enabled\n");
		return -ENODEV;
	}

	nina_kobj = kobject_create_and_add("nina", firmware_kobj);
	if (!nina_kobj) {
		printk(KERN_WARNING "%s: kset create error\n", __func__);
		nina_kobj = NULL;
	}
	nina_os_initialize1();
	acpi_load_tables();
        
//	status = acpi_enable_subsystem(ACPI_NO_ACPI_ENABLE);
//	if (ACPI_FAILURE(status)) {
//		pr_err("NINA Unable to start the ACPI Interpreter\n");
//		acpi_terminate();
//		return -ENODEV;
//	}

        //nina_sysfs_init();
	printk("nina init junho\n");
        nina_scan_init();	
return 0;
}

subsys_initcall(nina_init);
