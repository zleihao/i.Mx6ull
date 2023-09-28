#include "linux/export.h"
#include "linux/gfp.h"
#include "linux/of.h"
#include "linux/printk.h"
#include "linux/slab.h"
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
#include <linux/spi/spi.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include "icm20608reg.h"

/**
 * <0>: 不使用 misc
 * <1>:  使用 misc
*/
#define IS_USE_MISC 0

struct icm20608_device {
#if (IS_USE_MISC == 0)
    int major;   //主设备号
    int minor;   //次设备号
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    char name[20];
#endif

    void *private_data; /* 私有数据 */
    /* 片选引脚 */
    int cs_gpio;
    struct device_node *nd;
};

struct icm20608_device icm20608_dev = {
#if (IS_USE_MISC == 0)
    .major = 0,
    .name = "hao_icm20608",
#endif
};

/* spi 读写寄存器 */
static int icm20608_read_regs(struct icm20608_device *dev, u8 reg, void *val, int len)
{
    int ret = 0;
    u8 data = 0;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    data = reg | 0x80;
    spi_write_then_read(spi, &data, 1, val, len);

    return ret;
}

static s32 icm20608_write_regs(struct icm20608_device *dev, u8 reg, u8 *buf, int len)
{
   int ret = 0;
    u8 *tx_data;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    /* 申请内存 */
    tx_data = kzalloc(len+1, GFP_KERNEL);

    tx_data[0] = reg & ~0x80;
    memcpy(&tx_data[1], buf, len);

    spi_write(spi, tx_data, len+1);

    /* 释放内存 t */
    kzfree(tx_data);

    return ret; 
}

/* spi 读写单个寄存器 */
static unsigned char icm20608_read_onereg(struct icm20608_device *dev, u8 reg)
{
    unsigned char data = 0;
    icm20608_read_regs(dev, reg, &data, 1);

    return data;
}

static void icm20608_write_onereg(struct icm20608_device *dev, u8 reg, u8 data)
{
    icm20608_write_regs(dev, reg, &data, 1);
}

/*------------------ 设备操作函数 ----------------------*/
//chr open
static int icm20608_open(struct inode* inode, struct file* filp)
{
    u8 regvalue = 0;
    /* 私有数据 */
    filp->private_data = &icm20608_dev;

    icm20608_write_onereg(&icm20608_dev, ICM20_PWR_MGMT_1, 0x80);		/* 复位，复位后为0x40,睡眠模式 			*/
	mdelay(50);
	icm20608_write_onereg(&icm20608_dev, ICM20_PWR_MGMT_1, 0x01);		/* 关闭睡眠，自动选择时钟 					*/
	mdelay(50);

    //获取芯片ID
	regvalue = icm20608_read_onereg(&icm20608_dev, ICM20_WHO_AM_I);
    printk("ICM20608 ID = %#x\r\n", regvalue);

    regvalue = icm20608_read_onereg(&icm20608_dev, ICM20_PWR_MGMT_1);
    printk("ICM20_PWR_MGMT_1 = %#x\r\n", regvalue);

    return 0;
}

//chr write
static ssize_t icm20608_write(struct file* filp, const char __user* buf, size_t count, loff_t* ppos)
{
    return 0;
}

//chr read
static ssize_t icm20608_read(struct file* filp, char __user* buf, size_t count, loff_t* ppos)
{
	struct icm20608_device *dev = (struct icm20608_device *)filp->private_data;
	

	return 0;

}

//close chr dev
static int icm20608_close(struct inode* inode, struct file* filp)
{
    return 0;
}

/* 设备操作函数集 */
static struct file_operations icm20608_fops = {
    .owner    =  THIS_MODULE,
    .open     =  icm20608_open,
    .read     =  icm20608_read,
    .write    =  icm20608_write,
    .release  =  icm20608_close,
};

#if (IS_USE_MISC)
struct miscdevice icm20608_misc = {
    .minor = 255,
    .name = "hao_icm20608",
    .fops = &icm20608_fops,
};
#endif

int icm20608_probe(struct spi_device *spi)
{
    int err = 0;
    printk("icm20608 probe\r\n");

#if (IS_USE_MISC == 0)
    /* 1、创建设备号 */
    if (icm20608_dev.major) {  /* 定义了设备号 */
        icm20608_dev.devid = MKDEV(icm20608_dev.major, icm20608_dev.minor);  //得到设备号
        //自己提供
        err = register_chrdev_region(icm20608_dev.devid, 1, icm20608_dev.name);
    } else {   /* 没有定义设备号 */
        err = alloc_chrdev_region(&icm20608_dev.devid, 0, 1, icm20608_dev.name);  /* 申请设备号，次设备号为:0 */
        icm20608_dev.major = MAJOR(icm20608_dev.devid);      /* 获取分配号的主设备号 */
        icm20608_dev.minor = MINOR(icm20608_dev.devid);      /* 获取分配号的次设备号 */
    }

    if (err < 0) {
        printk("icm20608_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", icm20608_dev.major, icm20608_dev.minor);

    /* 2、初始化cdev */
    icm20608_dev.cdev.owner = THIS_MODULE;
    cdev_init(&icm20608_dev.cdev, &icm20608_fops);

    /* 3、添加一个cdev */
    err = cdev_add(&icm20608_dev.cdev, icm20608_dev.devid, 1);
    if (err < 0) {
        goto fail_cdev;
    }

    /* 4、创建类 */
    icm20608_dev.class = class_create(THIS_MODULE, icm20608_dev.name);
    if (IS_ERR(icm20608_dev.class)) {
        err = PTR_ERR(icm20608_dev.class);
        goto fail_class;
    }

    /* 5、创建设备 */
    icm20608_dev.device = device_create(icm20608_dev.class, NULL, icm20608_dev.devid, NULL, icm20608_dev.name);
    if (IS_ERR(icm20608_dev.device)) {
        err = PTR_ERR(icm20608_dev.device);
        goto fail_device;
    }
#else
    misc_register(&icm20608_misc);

#endif

    /* 初始化 spi */
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    /* 设置私有数据 */
    icm20608_dev.private_data = spi;

    return 0;

#if (IS_USE_MISC == 0)
fail_device:
    class_destroy(icm20608_dev.class);
fail_class:
    cdev_del(&icm20608_dev.cdev);
fail_cdev:
    unregister_chrdev_region(icm20608_dev.devid, 1);
fail_devid:
    return err;
#endif
}

int	icm20608_remove(struct spi_device *spi)
{
    printk("icm20608 remove\r\n");

#if (IS_USE_MISC == 0)
     /* 删除cdev */
    cdev_del(&icm20608_dev.cdev);

    /* 删除设备 */
    device_destroy(icm20608_dev.class, icm20608_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(icm20608_dev.devid, 1);

    /* 删除类 */
    class_destroy(icm20608_dev.class);
#else
    misc_deregister(&icm20608_misc);
#endif

    return 0;
}

/* 获取设备树 */
static struct of_device_id icm20608_of_device[] = {
    { .compatible = "invensense,icm20608", },
    {  }
};

/* 无设备树 */
static struct spi_device_id icm20608_derice_id = {
    .name = "invensense,icm20608",
};

/* icm20608驱动结构体 */
static struct spi_driver icm20608_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "icm20608",
        .of_match_table =  icm20608_of_device, //有设备树
    },
    //无设备树匹配
    .id_table = &icm20608_derice_id,
    .probe = icm20608_probe,
    .remove = icm20608_remove,
};

/* 驱动入口、出口 */
static int __init icm20608_init(void)
{

    return spi_register_driver(&icm20608_driver);
}

static void __exit icm20608_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

/* 加载、注销 */
module_init(icm20608_init);
module_exit(icm20608_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");