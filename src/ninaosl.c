#include <linux/kernel.h>     
#include <linux/init.h> 
#include <linux/kobject.h>
#include <linux/acpi.h>


#define OSI_STRING_LENGTH_MAX	64
#define OSI_STRING_ENTRIES_MAX	16

struct acpi_osi_entry {
	char string[OSI_STRING_LENGTH_MAX];
	bool enable;
};

static struct acpi_osi_config {
	u8		default_disabling;
	unsigned int	linux_enable:1;
	unsigned int	linux_dmi:1;
	unsigned int	linux_cmdline:1;
	unsigned int	darwin_enable:1;
	unsigned int	darwin_dmi:1;
	unsigned int	darwin_cmdline:1;
} osi_config;

static struct acpi_osi_config osi_config;
static struct acpi_osi_entry osi_setup_entries[OSI_STRING_ENTRIES_MAX] __initdata = {
	{"Module Device", true},
	{"Processor Device", true},
	{"3.0 _SCP Extensions", true},
	{"Processor Aggregator Device", true},
	/*
	 * Linux-Dell-Video is used by BIOS to disable RTD3 for NVidia graphics
	 * cards as RTD3 is not supported by drivers now.  Systems with NVidia
	 * cards will hang without RTD3 disabled.
	 *
	 * Once NVidia drivers officially support RTD3, this _OSI strings can
	 * be removed if both new and old graphics cards are supported.
	 */
	{"Linux-Dell-Video", true},
	/*
	 * Linux-Lenovo-NV-HDMI-Audio is used by BIOS to power on NVidia's HDMI
	 * audio device which is turned off for power-saving in Windows OS.
	 * This power management feature observed on some Lenovo Thinkpad
	 * systems which will not be able to output audio via HDMI without
	 * a BIOS workaround.
	 */
	{"Linux-Lenovo-NV-HDMI-Audio", true},
	/*
	 * Linux-HPI-Hybrid-Graphics is used by BIOS to enable dGPU to
	 * output video directly to external monitors on HP Inc. mobile
	 * workstations as Nvidia and AMD VGA drivers provide limited
	 * hybrid graphics supports.
	 */
	{"Linux-HPI-Hybrid-Graphics", true},
};


static struct workqueue_struct *kacpid_wq;
static struct workqueue_struct *kacpi_notify_wq;
static struct workqueue_struct *kacpi_hotplug_wq;


static u32 nina_osi_handler(acpi_string interface, u32 supported)
{
	if (!strcmp("Linux", interface)) {
		pr_notice_once(FW_BUG
			"BIOS _OSI(Linux) query %s%s\n",
			osi_config.linux_enable ? "honored" : "ignored",
			osi_config.linux_cmdline ? " via cmdline" :
			osi_config.linux_dmi ? " via DMI" : "");
	}
	if (!strcmp("Darwin", interface)) {
		pr_notice_once(
			"BIOS _OSI(Darwin) query %s%s\n",
			osi_config.darwin_enable ? "honored" : "ignored",
			osi_config.darwin_cmdline ? " via cmdline" :
			osi_config.darwin_dmi ? " via DMI" : "");
	}

	return supported;
}

static void __init nina_osi_setup_late(void)
{
	struct acpi_osi_entry *osi;
	char *str;
	int i;
	acpi_status status;

	if (osi_config.default_disabling) {
		status = acpi_update_interfaces(osi_config.default_disabling);
		if (ACPI_SUCCESS(status))
			pr_info("Disabled all _OSI OS vendors%s\n",
				osi_config.default_disabling ==
				ACPI_DISABLE_ALL_STRINGS ?
				" and feature groups" : "");
	}

	for (i = 0; i < OSI_STRING_ENTRIES_MAX; i++) {
		osi = &osi_setup_entries[i];
		str = osi->string;
		if (*str == '\0')
			break;
		if (osi->enable) {
			status = acpi_install_interface(str);
			if (ACPI_SUCCESS(status))
				pr_info("Added _OSI(%s)\n", str);
		} else {
			status = acpi_remove_interface(str);
			if (ACPI_SUCCESS(status))
				pr_info("Deleted _OSI(%s)\n", str);
		}
	}
}


int __init nina_osi_init(void)
{
	acpi_install_interface_handler(nina_osi_handler);
	 nina_osi_setup_late();

	return 0;
}

acpi_status __init nina_os_initialize1(void)
{
	
	
	kacpid_wq = alloc_workqueue("kacpid", 0, 1);
	kacpi_notify_wq = alloc_workqueue("kacpi_notify", 0, 1);
	kacpi_hotplug_wq = alloc_ordered_workqueue("kacpi_hotplug", 0);
	BUG_ON(!kacpid_wq);
	BUG_ON(!kacpi_notify_wq);
	BUG_ON(!kacpi_hotplug_wq);
	nina_osi_init();	
	return AE_OK;
}
