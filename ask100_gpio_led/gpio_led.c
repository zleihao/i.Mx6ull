#include "asm/gpio.h"
#include "linux/miscdevice.h"
#include "linux/printk.h"
#include "uapi/asm/stat.h"
#include <linux/types.h>                                                                                     
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/gpio/consumer.h>

static struct gpio_desc *gpioled;

struct file_operations gpioled_fops = {
    /* */
};

static struct miscdevice gpioled_misc = {
    .name = "ask100_gpio_led",
    .minor = 255,
    .fops = &gpioled_fops,
};

/* 有设备树 */
static struct of_device_id of_gpioled_device_id[] = {
    {
     .name = "myled", 
     .compatible = "ask100_gpio_led_old"
    },
    {  }
};

/* 没有设备树 */
static struct platform_device_id gpioled_device_id = {
    .name = "gpio_led",
};

/* gpio led初始化 */
static int gpio_led(struct platform_device *dev)
{
    /* 使用新版api */
    gpioled = gpiod_get(&dev->dev, "led", 0);

    if (IS_ERR(gpioled)) {
		dev_err(&dev->dev, "Failed to get GPIO for led\n");
		return -2;
	}

    /* 设置方向 */
    gpiod_direction_output(gpioled, 0);

    return 0;
}

int gpio_led_probe(struct platform_device *dev)
{
    int ret = 0;

    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    misc_register(&gpioled_misc);
    
    printk("minor = %d\r\n", gpioled_misc.minor);

    ret = gpio_led(dev);

    return 0;
}
	
int gpio_led_remove(struct platform_device *dev)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    gpiod_set_value(gpioled, 1);
    
    gpiod_put(gpioled);

    misc_deregister(&gpioled_misc);

    return 0;
}

static struct platform_driver gpio_led_driver = {
    .probe = gpio_led_probe,
    .remove = gpio_led_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "ask100_gpio_led",
        .of_match_table = of_gpioled_device_id,
    },
    .id_table = &gpioled_device_id,
};

//入口
static int __init gpio_led_init(void)
{
    return platform_driver_register(&gpio_led_driver);
}

//出口
static void __exit gpio_led_exit(void)
{
    platform_driver_unregister(&gpio_led_driver);
}

//注册、卸载
module_init(gpio_led_init);
module_exit(gpio_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");