#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "../ninabus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edson Juliano Drosdec");
MODULE_DESCRIPTION("A simple nina driver");
MODULE_VERSION("0.1");


int nina_misc_probe(struct nina_device *dev)
{
	printk("NINA driver probe\n");
	return 0;

}


struct nina_driver nina = {
	.type = "nina",
	.probe = nina_misc_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "nina-hello",
	},
};

static int __init hello_start(void)
{
	 int err;

	 err = nina_register_driver(&nina);
	 if(err) {
		pr_err("NINA unable to register driver %d\n",err);
		return err;
	 }
	
	 return 0;
}

static  void __exit hello_end(void)
{
	printk(KERN_INFO,"Goodbye NINA\n");

}

module_init(hello_start);
module_exit(hello_end);
