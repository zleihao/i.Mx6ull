#include "linux/printk.h"
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
#include <linux/miscdevice.h>

#define BEEP_ON 0
#define BEEP_OFF 1

struct beep_derive {
	struct device_node *nd;
	int gpio;
};
struct beep_derive beep_dev;

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
	struct beep_derive *dev = filp->private_data;

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

struct miscdevice misc_beep = {
	.name = "gpio_beep",
	.minor = 144,
	.fops = &beep_fop,
};

int beep_probe(struct platform_device *dev)
{
	int err = 0;

	printk("beep probe\r\n");

	/* 获取节点 */
	beep_dev.nd = dev->dev.of_node;
	if (beep_dev.nd == NULL) {
		printk("Can't beep node\r\n");
		err = -EINVAL;
		goto FAIL_FIND_NODE;
	}
	printk("node: %s\r\n", beep_dev.nd->name);

	/* 得到gpio */
	beep_dev.gpio = of_get_named_gpio(beep_dev.nd, "beep-gpios", 0);
	if (beep_dev.gpio < 0) {
		printk("Can't get beep gpio\r\n");
		err = -EINVAL;
		goto FAIL_FIND_NODE;
	}
	printk("beep gpio is:%d\r\n", beep_dev.gpio);

	/* 申请gpio 资源 */
	err = gpio_request(beep_dev.gpio, "imx6ull-beep");
	if (err) {
		printk("beep gpio request err\r\n");
		err = -EINVAL;
		goto FAIL_FIND_NODE;
	}
	printk("beep gpio request success\r\n");

	/* 设置 gpio 方向 */
	err = gpio_direction_output(beep_dev.gpio, 1);
	if (err) {
		pr_err("Failed set beep gpio direction\r\n");
		goto out;
	}

	/* misc init */
	err = misc_register(&misc_beep);
	if (err) {
		printk(KERN_WARNING "ac.o: Unable to register misc device\n");
		goto out;
	}

	return 0;

out:
	gpio_free(beep_dev.gpio);
FAIL_FIND_NODE:
	return err;
}

int beep_remove(struct platform_device *dev)
{
	printk("beep remove\r\n");

	/* 关闭 beep */
	gpio_set_value(beep_dev.gpio, 1);

	gpio_free(beep_dev.gpio);

	misc_deregister(&misc_beep);

	return 0;
}

struct of_device_id of_beep_match[] = {
	{ .compatible = "alientek,gpio_beep", },
	{ /* sentinel */ }
};

static struct platform_driver misc_beep_driver = {
	.driver = {
		.name = "imx6ull-beep",
		.of_match_table = of_beep_match,
	},
	.probe = beep_probe,
	.remove = beep_remove,
};

/* 驱动入口、出口 */
static int __init misc_beep_init(void)
{
	return platform_driver_register(&misc_beep_driver);
}

static void __exit misc_beep_exit(void)
{
	platform_driver_unregister(&misc_beep_driver);
}

module_init(misc_beep_init);
module_exit(misc_beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");