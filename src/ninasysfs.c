#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
//#include <acpi/actypes.h>
#include <linux/sysfs.h>
//#include <acpi/platform/aclinux.h>

#include <linux/acpi.h>

extern struct kobject *nina_kobj;

static LIST_HEAD(nina_table_attr_list);
static struct kobject *tables_kobj;
static struct kobject *tables_data_kobj;
static struct kobject *dynamic_tables_kobj;
static struct kobject *hotplug_kobj;

struct nina_table_attr {
	struct bin_attribute attr;
	char name[ACPI_NAMESEG_SIZE];
	int instance;
	char filename[ACPI_NAMESEG_SIZE+4];
	struct list_head node;

};

struct nina_data_attr{
	struct bin_attribute attr;
	u64 addr;
};

static ssize_t  nina_table_show(struct file *filp, struct kobject *kobj,
		                struct bin_attribute *bin_attr, char *buf,
				loff_t offset, size_t count)
{
	struct nina_table_attr *table_attr = 
		container_of(bin_attr, struct nina_table_attr,attr);
	struct acpi_table_header *table_header = NULL;
	acpi_status status;
	ssize_t rc;
         printk("NINa, inicio show table\n");
	status = acpi_get_table(table_attr->name,table_attr->instance,
			         &table_header);
	if (ACPI_FAILURE(status))
		return -ENODEV;
       	rc = memory_read_from_buffer(buf, count, &offset, table_header, 
			             table_header->length);
	acpi_put_table(table_header);	
	printk("NINA final show table\n");
	return rc;

}

static ssize_t nina_data_show(struct file *filp, struct kobject *kobj,
		              struct bin_attribute *bin_attr, char *buf,
			      loff_t offset, size_t count)
{
	struct nina_data_attr *data_attr;
	void __iomem *base;
	ssize_t rc;

	data_attr = container_of(bin_attr, struct nina_data_attr, attr);

	base = acpi_os_map_memory(data_attr->addr,data_attr->attr.size);
	if (!base)
		return -ENOMEM;

	rc = memory_read_from_buffer(buf,count,&offset,base,
			             data_attr->attr.size);
	acpi_os_unmap_memory(base, data_attr->attr.size);

	return rc;
}

static int nina_bert_data_init(void *th, struct nina_data_attr *data_attr)
{
	struct acpi_table_bert *bert = th;
	
	if (bert->header.length < sizeof(struct acpi_table_bert) ||
	    bert->region_length < sizeof(struct acpi_hest_generic_status)){
		kfree(data_attr);
		return -EINVAL;
	}
	data_attr->addr = bert->address;	
	data_attr->attr.size = bert->region_length;
	data_attr->attr.attr.name = "BERT";

	return sysfs_create_bin_file(tables_data_kobj,&data_attr->attr);	

}

static int nina_table_attr_init(struct kobject *tables_obj,
				struct nina_table_attr *table_attr,
				struct acpi_table_header *table_header)
{
	struct acpi_table_header *header = NULL;
	struct nina_table_attr *attr = NULL;
	char instance_str[4];

                 printk("NINA %s %d\n",__FILE__,__LINE__);	
	sysfs_attr_init(&table_attr->attr.attr);

	ACPI_COPY_NAMESEG(table_attr->name,table_header->signature);

	list_for_each_entry(attr, &nina_table_attr_list, node){
		if (ACPI_COMPARE_NAMESEG(table_attr->name,attr->name))
			if (table_attr->instance < attr->instance)
				table_attr->instance = attr->instance;
	
	}

	table_attr->instance++;

	if (table_attr->instance > 999){
		pr_warn("%4.4s: too many table instances\n",table_attr->name);
		return -ERANGE;
	}

	ACPI_COPY_NAMESEG(table_attr->filename, table_header->signature);
	table_attr->filename[ACPI_NAMESEG_SIZE] = '\0';
	if (table_attr->instance > 1 || (table_attr->instance == 1 &&
				         !acpi_get_table
					 (table_header->signature, 2, &header))){
		snprintf(instance_str,sizeof(instance_str), "%u",
			 table_attr->instance);
		strcat(table_attr->filename, instance_str);
	}

	table_attr->attr.size = table_header->length;
	table_attr->attr.read = nina_table_show;
	table_attr->attr.attr.name = table_attr->filename;
	table_attr->attr.attr.mode = 0400;

	return sysfs_create_bin_file(tables_obj, &table_attr->attr);

return 0;
}

static struct nina_data_obj {
	char *name;
	int (*fn) (void *, struct nina_data_attr *);
} nina_data_objs[] = {
	{ ACPI_SIG_BERT,nina_bert_data_init },
};

#define NUM_ACPI_DATA_OBJS ARRAY_SIZE(nina_data_objs)

static int nina_tables_data_init(struct acpi_table_header *th)
{
	struct nina_data_attr *data_attr;
	int i;
	
	for (i=0; i < NUM_ACPI_DATA_OBJS; i++) {
		if (ACPI_COMPARE_NAMESEG(th->signature,nina_data_objs[i].name)){
			data_attr = kzalloc(sizeof(*data_attr),GFP_KERNEL);
			if (!data_attr)
				return -ENOMEM;
			sysfs_attr_init(&data_attr->attr.attr);
			data_attr->attr.read = nina_data_show;
			data_attr->attr.attr.mode = 0400;
			return nina_data_objs[i].fn(th,data_attr);
		}

	}

	return 0;
}
static int nina_tables_sysfs_init(void)
{
	struct nina_table_attr *table_attr;
	struct acpi_table_header *table_header = NULL;
	int table_index;
	acpi_status status;
	int ret;

                 printk("NINA %s %d\n",__FILE__,__LINE__);	
	tables_kobj = kobject_create_and_add("tables",nina_kobj);
	if (!tables_kobj)
		goto err;

	tables_data_kobj = kobject_create_and_add("data",tables_kobj);
	if(!tables_data_kobj)
		goto err_tables_data;

	dynamic_tables_kobj = kobject_create_and_add("dynamic",tables_kobj);
	if(!dynamic_tables_kobj)
		goto err_dynamic_tables;
 
	for (table_index = 0;; table_index++){
		status = acpi_get_table_by_index(table_index, &table_header);

                 printk("NINA %s %d\n",__FILE__,__LINE__);	
		if(status == AE_BAD_PARAMETER)
			break;
	
		if (ACPI_FAILURE(status))
			continue;
	
		table_attr = kzalloc(sizeof(*table_attr), GFP_KERNEL);
	
		if (!table_attr)
			return -ENOMEM;
                 printk("NINA %s %d\n",__FILE__,__LINE__);	
		ret = nina_table_attr_init(tables_kobj,
				           table_attr,table_header);
		if (ret) {
			kfree(table_attr);
			return ret;
		}

      		list_add_tail(&table_attr->node, &nina_table_attr_list);
		nina_tables_data_init(table_header);
	}

	kobject_uevent(tables_kobj, KOBJ_ADD);
	kobject_uevent(tables_data_kobj, KOBJ_ADD);
	kobject_uevent(dynamic_tables_kobj, KOBJ_ADD);
        
	printk("Nina final do metodo\n");
	return 0;

err_dynamic_tables:
	kobject_put(tables_data_kobj);
err_tables_data:
	kobject_put(tables_kobj);
err:
	return -ENOMEM;
}

static ssize_t
nina_show_profile(struct kobject *kobj, struct kobj_attribute *attr,
		  char *buf)
{
	return sprintf(buf, "%d\n",acpi_gbl_FADT.preferred_profile);
}

static ssize_t force_remove_show(struct kobject *kobj,
		                 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",0);
}

static ssize_t force_remove_store(struct kobject *kobj,
	       			  struct kobj_attribute *attr,
				  const char *buf, size_t size)
{
	bool val;
	int ret;

	ret = strtobool(buf, &val);
	if(ret < 0 )
		return ret;

	if (val){
		pr_err("Nina Enabling force_remove is not supported\n");
		return -EINVAL;
	}
	
	return size;
}

static const struct kobj_attribute pm_profile_attr = 
	__ATTR(pm_profile, S_IRUGO, nina_show_profile, NULL);

static const struct kobj_attribute force_remove_attr = 
	__ATTR(force_remove, S_IRUGO | S_IWUSR, force_remove_show,
			force_remove_store);

	
int __init nina_sysfs_init(void)
{
	int result;

                 printk("NINA %s %d\n",__FILE__,__LINE__);
	if (acpi_table_init())
		return 0;
        printk("drosdeck kkkkkkkkkkkkkk\n");
	result = nina_tables_sysfs_init();
	
	if(result)
		return result;

	hotplug_kobj = kobject_create_and_add("hotplug",nina_kobj);
	if (!hotplug_kobj)
		return -ENOMEM;
	
	result = sysfs_create_file(hotplug_kobj, &force_remove_attr.attr);
	if (result)
		return result;
	
	result = sysfs_create_file(nina_kobj, &pm_profile_attr.attr);

	printk("NINA sysfs\n");
	return  result;
}
