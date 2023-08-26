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

#define BEEP_ON 0
#define BEEP_OFF 1

/* beep结构体 */
struct gpio_beep_device {
    struct device_node *nd;
    int gpio;
    struct device *device;
    struct class *class;
    struct cdev cdev;
    dev_t devid;
    int major;
    int minor;

    char *name;
};
struct gpio_beep_device beep_dev = {
    .major = 0,
    .name = "beep"
};

static int beep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep_dev; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char beepstat;
	struct gpio_beep_device *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];		/* 获取状态值 */

	if(beepstat == BEEP_ON) {	
		gpio_set_value(dev->gpio, 0);	/* 打开蜂鸣器 */
	} else if(beepstat == BEEP_OFF) {
		gpio_set_value(dev->gpio, 1);	/* 关闭蜂鸣器 */
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int beep_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations beep_fop = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .write = beep_write,
    .release = beep_release,
};

/* 设备入口与出口 */
static int __init gpio_beep_init(void)
{
    int err = 0;

    /* 先获取beep节点 */
    beep_dev.nd = of_find_node_by_path("/gpio_beep");
    if (beep_dev.nd == NULL) {
        printk("Can't find gpio_beep node!!\r\n");
        err = -EINVAL;
        goto fail_find_nd;
    }

    /* 2、 获取设备树中的gpio属性，得到BEEP所使用的BEEP编号 */
    beep_dev.gpio = of_get_named_gpio(beep_dev.nd, "beep-gpios", 0);
    if (beep_dev.gpio < 0) {
        printk("Can't find beep-gpios property!\r\n");
        err = -EINVAL;
        goto fail_find_nd;
    } else {
        printk("beep gpio is %d\r\n", beep_dev.gpio);
    }

    /* 申请gpio */
    err = gpio_request(beep_dev.gpio, "beep-gpio");
    if (err < 0) {
        printk("gpio request err\r\n");
        err = -EINVAL;
        goto fail_find_nd;
    } else {
        printk("request success!\r\n");
    }

    /* 设置gpio输出方向 */
    err = gpio_direction_output(beep_dev.gpio, 1);
    if (err < 0) {
        printk("set gpio direction err!!\r\n");
        err = -EINVAL;
        goto fail_request;
    }
    
    /* 创建设备号 */
    if (beep_dev.major) {
        beep_dev.devid = MKDEV(beep_dev.major, beep_dev.minor);
        err = register_chrdev_region(beep_dev.devid, 1, beep_dev.name);
    } else {
        err = alloc_chrdev_region(&beep_dev.devid, 0, 1, beep_dev.name);
    }

    if (err) {
        printk("register faill!!\r\n");
        goto fail_request;
    } else {
        beep_dev.major = MAJOR(beep_dev.devid);
        beep_dev.minor = MINOR(beep_dev.minor);
        printk("beep dev major = %d, minor = %d\r\n", beep_dev.major, beep_dev.minor);
    }

    /* 初始化cdev */
    beep_dev.cdev.owner = THIS_MODULE;
    cdev_init(&beep_dev.cdev, &beep_fop);

    /* 添加cdev */
    err = cdev_add(&beep_dev.cdev, beep_dev.devid, 1);
    if (err < 0) {
        printk("Cdev add fail!!\r\n");
        goto fail_unregister;
    }

    /* 创建class */
    beep_dev.class = class_create(THIS_MODULE, beep_dev.name);
    if (IS_ERR(beep_dev.class)) {
		err = PTR_ERR(beep_dev.class);
        goto fail_cdev_del;
	}

    /* 添加device */
    beep_dev.device = device_create(beep_dev.class, NULL, beep_dev.devid, NULL, beep_dev.name);
    if (IS_ERR(beep_dev.device)) {
        err = PTR_ERR(beep_dev.device);
        goto fail_class_destroy;
    }

    return 0;

fail_class_destroy:
    class_destroy(beep_dev.class);
fail_cdev_del:
    cdev_del(&beep_dev.cdev);
fail_unregister:
    unregister_chrdev_region(beep_dev.devid, 1);
fail_request:
    gpio_free(beep_dev.gpio);
fail_find_nd:
    return err;
}

static void __exit gpio_beep_exit(void)
{
    /* 关闭蜂鸣器 */
    gpio_set_value(beep_dev.gpio, 1);

    gpio_free(beep_dev.gpio);

    device_destroy(beep_dev.class, beep_dev.devid);
    unregister_chrdev_region(beep_dev.devid, 1);
    class_destroy(beep_dev.class);
    
    cdev_del(&beep_dev.cdev);
}

/* 加载、注销 */
module_init(gpio_beep_init);
module_exit(gpio_beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");