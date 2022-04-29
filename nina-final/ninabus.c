#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>

struct acpi_device *nina_root;

int nina_bus(int i)
{
printk("nina bus");
return 0;
}
