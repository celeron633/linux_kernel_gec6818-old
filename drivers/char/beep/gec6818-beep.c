/*
 * (C) 2011-2013 by xboot.org
 * Author: jianjun jiang <jerryjianjun@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <asm/gpio.h>
#include <asm/delay.h>
#include <linux/clk.h>
#include <mach/gpio.h>
#include <mach/soc.h>
#include <mach/platform.h>

/*
 * BEEP -> PWM2 -> GPDC(14)
 */
#define	CFG_IO_GEC6818_BEEP				(PAD_GPIO_C + 14)
static int __GEC6818_beep_status = 0;

static void __GEC6818_beep_probe(void)
{
	gpio_request(CFG_IO_GEC6818_BEEP, "GEC6818-beep");
	gpio_direction_output(CFG_IO_GEC6818_BEEP, 0);

	__GEC6818_beep_status = 0;
}

static void __GEC6818_beep_remove(void)
{
	gpio_free(CFG_IO_GEC6818_BEEP);
}

static ssize_t GEC6818_beep_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(!strcmp(attr->attr.name, "state"))
	{
		if(__GEC6818_beep_status != 0)
			return strlcpy(buf, "1\n", 3);
		else
			return strlcpy(buf, "0\n", 3);
	}
	return strlcpy(buf, "\n", 3);
}

static ssize_t GEC6818_beep_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);

	if(!strcmp(attr->attr.name, "state"))
	{
		if(on)
		{
			gpio_direction_output(CFG_IO_GEC6818_BEEP, 1);
			__GEC6818_beep_status = 1;
		}
		else
		{
			gpio_direction_output(CFG_IO_GEC6818_BEEP, 0);
			__GEC6818_beep_status = 0;
		}
	}

	return count;
}

static DEVICE_ATTR(state, 0666, GEC6818_beep_read, GEC6818_beep_write);

static struct attribute * GEC6818_beep_sysfs_entries[] = {
	&dev_attr_state.attr,
	NULL,
};

static struct attribute_group GEC6818_beep_attr_group = {
	.name	= NULL,
	.attrs	= GEC6818_beep_sysfs_entries,
};

static int GEC6818_beep_probe(struct platform_device *pdev)
{
	__GEC6818_beep_probe();

	return sysfs_create_group(&pdev->dev.kobj, &GEC6818_beep_attr_group);
}

static int GEC6818_beep_remove(struct platform_device *pdev)
{
	__GEC6818_beep_remove();

	sysfs_remove_group(&pdev->dev.kobj, &GEC6818_beep_attr_group);
	return 0;
}

#ifdef CONFIG_PM
static int GEC6818_beep_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int GEC6818_beep_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define GEC6818_beep_suspend	NULL
#define GEC6818_beep_resume	NULL
#endif

static struct platform_driver GEC6818_beep_driver = {
	.probe		= GEC6818_beep_probe,
	.remove		= GEC6818_beep_remove,
	.suspend	= GEC6818_beep_suspend,
	.resume		= GEC6818_beep_resume,
	.driver		= {
		.name	= "GEC6818-beep",
	},
};

static struct platform_device GEC6818_beep_device = {
	.name      = "GEC6818-beep",
	.id        = -1,
};

static int __devinit GEC6818_beep_init(void)
{
	int ret;

	printk("GEC6818 beep driver\r\n");

	ret = platform_device_register(&GEC6818_beep_device);
	if(ret)
		printk("failed to register GEC6818 beep device\n");

	ret = platform_driver_register(&GEC6818_beep_driver);
	if(ret)
		printk("failed to register GEC6818 beep driver\n");

	return ret;
}

static void GEC6818_beep_exit(void)
{
	platform_driver_unregister(&GEC6818_beep_driver);
}

module_init(GEC6818_beep_init);
module_exit(GEC6818_beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jianjun jiang <jerryjianjun@gmail.com>");
MODULE_DESCRIPTION("GEC6818 beep driver");
