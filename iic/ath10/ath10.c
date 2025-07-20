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
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/miscdevice.h>
#include "acpi/acoutput.h"
#include "asm-generic/int-ll64.h"
#include "linux/export.h"
#include "linux/printk.h"
#include "uapi/linux/i2c.h"
#include <linux/delay.h>
#include <linux/miscdevice.h>

struct ath10_driver_set {
	struct i2c_client *ath10_client;
};
static struct ath10_driver_set ath10_driver;

static int ath10_open(struct inode *inode, struct file *file)
{
	unsigned char status = 0;

	printk("%s %s %d\n", __FILE__, __func__, __LINE__);
	
	/*
		1. 先写入 x071
		2. 读取数据
		3. bit3不为1时需要发送初始化指令。
           bit7为1时，表示设备是忙状态，此时不能读取数据
	 */
    i2c_smbus_write_byte(ath10_driver.ath10_client, 0x71);
	status = i2c_smbus_read_byte(ath10_driver.ath10_client);

	if (0 == (status & 0x08)) {
		/*
			1. 得到 ath10 的状态
			2. 判断 bit3 是否为 1，如果不为 1，则需要先进行校准
			2.1 校准方法：向 ath10 依次写入: 0xe1 0x08 0x00
		*/
		printk("ath10 need init...\n");
		//校准 ath10
	}

	printk("not need init...\n");
	
	return 0;
}

static ssize_t ath10_read(struct file* filp, char __user* buf, size_t count, loff_t* ppos)
{
	/*
		1. 先触发测量温度
		 1.1 向设备写入 0xac 0x33 0x00 
		2. 等待80ms
		3. 读取数据
	*/
	int ret = 0;
	unsigned char ans[6];
	unsigned char data[4]= { 0xac, 0x33, 0x00 };
	struct i2c_msg msgs;

	if (count != 5) {
		return -EINVAL;
	}

	//把ath10的地址放到数据的最开始
	msgs.addr = ath10_driver.ath10_client->addr;
	msgs.flags = 0;
	msgs.buf = data;
	msgs.len = 3;

	ret = i2c_transfer(ath10_driver.ath10_client->adapter, &msgs, 1);
	if (ret < 0) {
		printk("============== ret = %d\n", ret);
		return ret;
	}

	//等待 80ms
	mdelay(80);

	unsigned char reg = 0x00;
	//读取数据
	struct i2c_msg read_msg = {
		.addr = ath10_driver.ath10_client->addr,
		.flags = ath10_driver.ath10_client->flags | I2C_M_RD,
		.len = 6,
		.buf = ans,
	};

	ret = i2c_transfer(ath10_driver.ath10_client->adapter, &read_msg, 1);
	if (ret < 0) {
		printk("----------------\n");
		return ret;
	}

	if (ans[0] & 0x80) {
		//读取失败
		printk("%x\n", ans[0]);
		return -EINVAL;
	}

	//读取成功
	copy_to_user(buf, &ans[1], 5);
	
	return 0;
}


static const struct file_operations bcm63xx_wdt_fops = {
	.owner		= THIS_MODULE,
	.open		= ath10_open,
	.read       = ath10_read,
};

/* misc 设备 */
static struct miscdevice ath10_miscdev = {
	.minor	= 255,
	.name	= "ath10",
	.fops	= &bcm63xx_wdt_fops,
};


int ath10_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	printk("%s %s %d\n", __FILE__, __func__, __LINE__);

	err = misc_register(&ath10_miscdev);
	if (err) {
		printk("misc_register fail!");
		goto misc_fail;
	}

	ath10_driver.ath10_client = client;
	
	return 0;
misc_fail:
	return err;
}

int ath10_i2c_remove(struct i2c_client *client)
{
	printk("%s %s %d\n", __FILE__, __func__, __LINE__);

	misc_deregister(&ath10_miscdev);

	return 0;
}

struct of_device_id ath10_i2c_of_device_id[] = {
	{.name = "ath10", .compatible = "aosong,ath10"},
	{}
};

static const struct i2c_device_id ath10_i2c_ids[] = {
	{ .name = "ath10" },
	{}
};


static struct i2c_driver ath10_i2c_driver = {
	.driver = {
        .owner = THIS_MODULE,
        .name = "ath10",
        .of_match_table = ath10_i2c_of_device_id,
    },
	.probe = ath10_i2c_probe,
	.remove = ath10_i2c_remove,
	.id_table = ath10_i2c_ids,
};


static int __init ath10_init_module(void)
{
	return i2c_add_driver(&ath10_i2c_driver);
}


static void __exit ath10_cleanup_module (void)
{
	i2c_del_driver(&ath10_i2c_driver);
}


module_init(ath10_init_module);
module_exit(ath10_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lighting master");



