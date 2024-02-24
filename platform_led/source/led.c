#include "linux/printk.h"
#include "linux/spinlock.h"
#include <asm-generic/errno-base.h>
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

//led字符设备相关
typedef struct led_chr_dev {
    struct cdev     cdev;
    struct class   *class;
    struct device  *device;   /* 设备 */
    struct gpio_desc *gpio;
    dev_t           devid;
    int             major;  //主设备号
    int             minor;  //次设备号
    char            name[20];
    unsigned char   flie_state;
    spinlock_t      lock; /* 自旋锁 */
}led_chr_dev_t;

static led_chr_dev_t  led_dev = {
    .flie_state = 1, 
    .major = 0,
    .name = "hao-led",
};

/* 设备字符操作 */
int led_open(struct inode *node, struct file *file)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    spin_lock(&led_dev.lock);

    if (!led_dev.flie_state) {
        spin_unlock(&led_dev.lock);

        return -EBUSY;
    }

    led_dev.flie_state--;

    spin_unlock(&led_dev.lock);

    return 0;
}

ssize_t led_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    int err = 0;
    unsigned char data;

    spin_lock(&led_dev.lock);

    err = copy_from_user(&data, buf, 1);
    if (err < 0) {
        printk("Data for user err\r\n");
        return -1;
    }
    printk("form user data: %d\n", data);
    gpiod_set_value(led_dev.gpio, data);

    spin_unlock(&led_dev.lock);

    return 0;
}

int led_close(struct inode *node, struct file *file)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    spin_lock(&led_dev.lock);

    if (!led_dev.flie_state) {
        led_dev.flie_state++;
    }

    spin_unlock(&led_dev.lock);

    return 0;
}

struct file_operations led_fop = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_close,
    .write = led_write,
};

//注册设备号
static int create_led_chr_device(struct platform_device *pdev)
{
    int err = 0;

    /* 1, 申请设备号 */
    if (led_dev.major) {
        //自己提供设备号
        led_dev.devid = MKDEV(led_dev.major, led_dev.minor);
        err = register_chrdev_region(led_dev.devid, 1, led_dev.name);
    } else {
        //向内核申请
        err = alloc_chrdev_region(&led_dev.devid, 100, 1, led_dev.name);
        led_dev.major = MAJOR(led_dev.devid);
        led_dev.minor = MINOR(led_dev.devid);
    }

    if (err < 0) {
        printk("led_dev register fail!\r\n");
        goto FAIL_DEVID;
    }
    printk("major: %d, minor: %d\r\n", led_dev.major, led_dev.minor);

    /* 2，初始化cdv */
    led_dev.cdev.owner = THIS_MODULE;
    cdev_init(&led_dev.cdev, &led_fop);
    err = cdev_add(&led_dev.cdev, led_dev.devid, 1);
    if (err < 0) {
        printk("cdev add err\r\n");
        goto FAIL_CDEV_ADD;
    }

    /* 3,创建class */
    led_dev.class = class_create(THIS_MODULE, led_dev.name);
    if (IS_ERR(led_dev.class)) {
        err = PTR_ERR(led_dev.class);
        goto FAIL_CREATE_CLASS;
    }

    /* 4，添加设备到class */
    led_dev.device = device_create(led_dev.class, NULL, led_dev.devid, NULL, led_dev.name);
    if (IS_ERR(led_dev.device)) {
        err = PTR_ERR(led_dev.device);
        goto FAIL_CREATE_DEVICE;
    }

    /* 5,得到led gpio decs */
    led_dev.gpio = gpiod_get(&pdev->dev, "led", 0);
    if (IS_ERR(led_dev.gpio)) {
		printk("Failed to get GPIO for led\n");
		err = PTR_ERR(led_dev.gpio);
        goto FAIL_GET_GPIO;
    }
    //设置方向
    gpiod_direction_output(led_dev.gpio, 0);

    return 0;

FAIL_GET_GPIO:
    device_destroy(led_dev.class, led_dev.devid);
FAIL_CREATE_DEVICE:
    class_destroy(led_dev.class);
FAIL_CREATE_CLASS:
    cdev_del(&led_dev.cdev);
FAIL_CDEV_ADD:
    unregister_chrdev_region(led_dev.devid, 1);
FAIL_DEVID:
    return  err;
}

/* platform 相关 */
struct of_device_id led_of_device_id[] = {
    { .compatible = "alientek,gpio_led" },
    {  }
};

static int led_probe(struct platform_device *dev)
{
    int err = 0;

    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    /* 初始化自旋锁 */
    spin_lock_init(&led_dev.lock);

    err = create_led_chr_device(dev);
    if (err < 0) {
        return err;
    }

    return 0;
}

static int led_remove(struct platform_device *dev)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    gpiod_set_value(led_dev.gpio, 0);

    gpiod_put(led_dev.gpio);
    
    /* 删除cdev */
    cdev_del(&led_dev.cdev);

    /* 删除设备 */
    device_destroy(led_dev.class, led_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(led_dev.devid, 1);

    /* 删除类 */
    class_destroy(led_dev.class);

    return 0;
}

struct platform_driver led_driver = {
    .driver = {
        .name = "imx6ull-led",
        .of_match_table = led_of_device_id,
    },
    .probe = led_probe,
    .remove = led_remove,
};

//入口
static int __init led_init(void)
{
    return platform_driver_register(&led_driver);
}

//出口
static void __exit led_exit(void)
{
    platform_driver_unregister(&led_driver);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");