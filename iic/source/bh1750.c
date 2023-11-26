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

//Rom指令
#define POWER_ON           0x01
#define H_RESOLUTION_MODE  0x10

struct bh1750_device {
    void *private_data;
    u16 tmp;
};

static struct bh1750_device bh1750_dev;

static int bh1750_read_regs(struct bh1750_device *dev, void *val, int len)
{
    int ret = 0;
    struct i2c_client *client = (struct i2c_client *)dev->private_data; 

     struct i2c_msg msgs[1] = {
        [0] = {  //寄存器的值
            .addr = client->addr,
            .flags = I2C_M_RD,
            .buf = val,
            .len = len,
        }
    };

    ret = i2c_transfer(client->adapter, msgs, 1);

    if (ret != 1) {
        printk("i2c rd failed=%d len=%d\n",ret, len);
        ret = -EINVAL;
    } else {
        ret = 0;
    }

    return ret;
}

/* bh1750写数据 */
static int bh1750_write_regs(struct bh1750_device *dev, u8 *buf, u8 len)
{
    struct i2c_client *client = (struct i2c_client *)dev->private_data;
    u8 data[256];

    struct i2c_msg msgs;

    memcpy(data, buf, len);

    msgs.addr = client->addr;
    msgs.flags = 0;
    msgs.buf = data;
    msgs.len = len;

    return i2c_transfer(client->adapter, &msgs, 1);
}

static void bh1750_write_reg(struct bh1750_device *dev, u8 data)
{
    int ret;

    ret = bh1750_write_regs(dev, &data, 1);
    if (ret != 1) {
        printk("i2c write fail: %d\n", ret);
    }
}

/* bh1750字符集 */
int bh1750_open(struct inode *inode, struct file *file)
{
    bh1750_write_reg(&bh1750_dev, POWER_ON);
    bh1750_write_reg(&bh1750_dev, H_RESOLUTION_MODE);
    mdelay(200);
    return 0;
}

ssize_t bh1750_read(struct file* filp, char __user* buf, size_t count, loff_t* ppos)
{
    u8 data[2];
    int err;

    bh1750_read_regs(&bh1750_dev, data, 2);


	err = copy_to_user(buf, data, sizeof(data));

    return 0;
}

int bh1750_close(struct inode *inode, struct file *file)
{
    return 0;
}

struct file_operations bh1750_fops = {
    .open = bh1750_open,
    .release = bh1750_close,
    .read = bh1750_read,
};

struct miscdevice bh1750_misc = {
    .name = "bh1750",
    .fops = &bh1750_fops,
    .minor = 255,
};

struct of_device_id bh1750_of_device_id[] = {
    { .compatible = "zlh,bh1750", },
    {  }
};

struct i2c_device_id bh1750_device_id = {
    .name = "zlh,bh1750",
};

int bh1750_probe(struct i2c_client *client, const struct i2c_device_id *dev)
{
    int ret;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    ret = misc_register(&bh1750_misc);
    if (ret < 0) {
        printk(KERN_WARNING "ac.o: Unable to register misc device\n");
        goto MISC_REGISTER_FAIL;
    }

    /* 获得iic_client */
    bh1750_dev.private_data = client;

    printk("bh1750 addr: %#x\r\n", client->addr);

    return 0;

MISC_REGISTER_FAIL:
    return ret;
}

int bh1750_remove(struct i2c_client *client)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    misc_deregister(&bh1750_misc);
    
    return 0;
}

struct i2c_driver bh1750_driver = {
    .id_table = &bh1750_device_id,
    .driver = {
        .owner = THIS_MODULE,
        .name = "bh1750",
        .of_match_table = bh1750_of_device_id,
    },
    .probe = bh1750_probe,
    .remove = bh1750_remove,
};

/* 入口 */
static int __init bh1750_driver_init(void)
{
    return i2c_add_driver(&bh1750_driver);
}

static void __exit bh1750_driver_exit(void)
{
    i2c_del_driver(&bh1750_driver);
}

module_init(bh1750_driver_init);
module_exit(bh1750_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");