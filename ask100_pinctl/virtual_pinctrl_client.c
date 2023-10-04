#include "linux/xz.h"
#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>

static const struct of_device_id virtual_client_of_match[] = {
	{ .compatible = "100ask,virtual_i2c", },
	{ },
};


int virtual_client_probe(struct platform_device *dev)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

int virtual_client_remove(struct platform_device *dev)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}



static struct platform_driver virtual_client_driver = {
	.probe		= virtual_client_probe,
	.remove		= virtual_client_remove,
	.driver		= {
		.name	= "100ask_virtual_client",
		.of_match_table = of_match_ptr(virtual_client_of_match),
	}
};

//入口
static int __init virtual_client_init(void)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return platform_driver_register(&virtual_client_driver);
}

static void __exit virtual_client_exit(void)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    platform_driver_unregister(&virtual_client_driver);
}

module_init(virtual_client_init);
module_exit(virtual_client_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");