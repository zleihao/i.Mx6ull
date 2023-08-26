#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/atomic.h>

#define LED_OFF    0  //关灯
#define LED_ON     1  //开灯

/* gpio驱动结构体 */
struct gpio_led_device {
    dev_t devid;
    struct device_node *nd;
    struct device *device;
    struct class *class;
    struct cdev cdev;
    int major;
    int minjor;
    int led_gpio;
    char *name;
    /* 原子操作 */
    atomic_t lock;
};
struct gpio_led_device gpio_led_dev = {
    .major = 0, /* 由内核申请 */
    .name = "gpio_led",
};

//led open
static int led_open(struct inode *inode, struct file *filp)
{
    int ret = 0;

    /* 私有数据 */
    filp->private_data = &gpio_led_dev;

    if (atomic_read(&gpio_led_dev.lock) <= 0) {
        return -EBUSY;
    } else {
        atomic_dec(&gpio_led_dev.lock);
    }

    return ret;
}

//led write
static ssize_t led_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
	u8 data_buf[1];

	if (count > 1) {
		printk("write data too long...\r\n");
		return -EPERM;
	}
	ret = copy_from_user(data_buf, buf, count);
	if (ret < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	/* 判断是开灯还是关灯 */
	switch (data_buf[0])
	{
		case LED_OFF:
			//设置IO值，置1
			gpio_set_value(gpio_led_dev.led_gpio, 1);
			break;
		case LED_ON:
		    //设置IO值，清0
			gpio_set_value(gpio_led_dev.led_gpio, 0);
			break;
		default:
			printk("using error!\r\n");
			break;
	}

	return 0;
}

//close led dev
static int led_close(struct inode *inode, struct file *filp)
{
    struct gpio_led_device *dev = (struct gpio_led_device *)filp->private_data;

    atomic_inc(&dev->lock);

    return 0;
}

struct file_operations gpio_led_fops = {
    .owner = THIS_MODULE, 
    .open = led_open,
    .write = led_write,
    .release = led_close,
};

/* 入口与出口 */
static int __init gpio_led_init(void)
{
    int ret = 0;

    /* 初始化原子变量 */
    atomic_set(&gpio_led_dev.lock, 1);

    if (gpio_led_dev.major) {
        gpio_led_dev.devid = MKDEV(gpio_led_dev.major, gpio_led_dev.minjor);
        ret = register_chrdev_region(gpio_led_dev.devid, 1, gpio_led_dev.name);
    } else {
        ret = alloc_chrdev_region(&gpio_led_dev.devid, 0, 1, gpio_led_dev.name);
    }
    if (ret < 0) {
        printk(KERN_ERR "alloc_chrdev_region failed\n");
        goto fail_dev;
    } else {
        gpio_led_dev.major = MAJOR(gpio_led_dev.devid);
        gpio_led_dev.minjor = MINOR(gpio_led_dev.devid);
        printk("major = %d, minjor = %d\r\n", gpio_led_dev.major, gpio_led_dev.minjor);
    }

    // cdev
    cdev_init(&gpio_led_dev.cdev, &gpio_led_fops);
    gpio_led_dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&gpio_led_dev.cdev, gpio_led_dev.devid, 1);
    if (ret < 0) {
        printk(KERN_ERR "cdev_add failed\n");
        goto fail_chrdev;
    }

    //class
    gpio_led_dev.class = class_create(THIS_MODULE, gpio_led_dev.name);
    if (IS_ERR(gpio_led_dev.class)) {
        ret = PTR_ERR(gpio_led_dev.class);
        goto fail_class;
    }

    //device
    gpio_led_dev.device = device_create(gpio_led_dev.class, NULL, gpio_led_dev.devid, NULL, gpio_led_dev.name);
    if (IS_ERR(gpio_led_dev.device)) {
        ret = PTR_ERR(gpio_led_dev.device);
        goto fail_device;
    }

    /* 获取节点gpio_led */
    gpio_led_dev.nd = of_find_node_by_path("/gpio_led");
    if (gpio_led_dev.nd == NULL) {
        goto fail_find_nd;
    } else {
        printk("gpio_led node find!\r\n");
    }

    /* 得到gpio属性 */
    gpio_led_dev.led_gpio = of_get_named_gpio(gpio_led_dev.nd, "led-gpios", 0);
    if (gpio_led_dev.led_gpio < 0) {
        printk("Can't find gpio info\r\n");
        goto fail_find_nd;
    } else {
        printk("gpio num:%d\r\n", gpio_led_dev.led_gpio);
    }

    /* 申请gpio */
    // ret = gpio_request(gpio_led_dev.led_gpio, "led-gpio");
    // if (ret) {
	// 	printk("Failed to request the led gpio\r\n");
	// 	goto fail_find_nd;
	// }

    /* 使用gpio */
    ret = gpio_direction_output(gpio_led_dev.led_gpio, 1);  //拉高电平
    if (ret) {
		printk("Failed to reset the led\r\n");
		goto led_free_reset;
	}

    /* 输出低电平，点亮led */
    gpio_set_value(gpio_led_dev.led_gpio, 1);

    return 0;

led_free_reset:
    // gpio_free(gpio_led_dev.led_gpio);
fail_find_nd:
    device_destroy(gpio_led_dev.class, gpio_led_dev.devid);
fail_device:
    class_destroy(gpio_led_dev.class);
fail_class:
    cdev_del(&gpio_led_dev.cdev);
fail_chrdev:
    unregister_chrdev_region(gpio_led_dev.devid, 1);
fail_dev:
    return ret;
}

static void __exit gpio_led_exit(void)
{
    /* 输出高电平，熄灭led */
    gpio_set_value(gpio_led_dev.led_gpio, 1);
    
    /* 释放gpio */
    // gpio_free(gpio_led_dev.led_gpio);

    cdev_del(&gpio_led_dev.cdev);
    
    unregister_chrdev_region(gpio_led_dev.devid, 1);

    device_destroy(gpio_led_dev.class, gpio_led_dev.devid);

    class_destroy(gpio_led_dev.class);
}

/* 挂载与注销 */
module_init(gpio_led_init);
module_exit(gpio_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");
