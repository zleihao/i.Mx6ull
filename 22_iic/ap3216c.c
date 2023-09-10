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
#include "ap3216creg.h"
#include "asm-generic/int-ll64.h"
#include "linux/export.h"
#include "linux/printk.h"
#include <linux/delay.h>

/**
 * <0>: 不使用 misc
 * <1>:  使用 misc
*/
#define IS_USE_MISC 0

struct ap3216c_device {
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
    unsigned short ir, als, ps;		/* 三个光传感器数据 */ 
};

struct ap3216c_device ap3216c_dev = {
#if (IS_USE_MISC == 0)
    .major = 0,
    .name = "hao_ap3216c",
#endif
};

/*------------------ ap3216c 读写寄存器-----------------*/
static int ap3216c_read_regs(struct ap3216c_device *dev, u8 reg, void *val, int len)
{
    int ret = 0;
    struct i2c_client *client = (struct i2c_client *)dev->private_data; 

    struct i2c_msg msgs[2] = {
        [0] = {  //寄存器地址
            .addr = client->addr,
            .flags = 0,
            .buf = &reg,
            .len = 1,
        },
        [1] = {  //寄存器的值
            .addr = client->addr,
            .flags = I2C_M_RD,
            .buf = val,
            .len = len,
        }
    };

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret != 2) {
        printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
        ret = -EINVAL;
    } else {
        ret = 0;
    }

    return ret;
}

static int ap3216c_write_regs(struct ap3216c_device *dev, u8 reg, u8 *buf, u8 len)
{
    struct i2c_client *client = (struct i2c_client *)dev->private_data;
    u8 data[256];

    struct i2c_msg msgs;

    data[0] = reg;
    memcpy(&data[1], buf, len);

    msgs.addr = client->addr;
    msgs.flags = 0;
    msgs.buf = data;
    msgs.len = len + 1;

    return i2c_transfer(client->adapter, &msgs, 1);
}

/*------------------ ap3216c 读写单个寄存器-----------------*/
static unsigned char ap3216c_read_reg(struct ap3216c_device *dev, u8 reg)
{
    unsigned char data;
    int err = 0;

    err = ap3216c_read_regs(dev, reg, &data, 1);
    if (err < 0) {

    }

    return data;
}

static void ap3216c_write_reg(struct ap3216c_device *dev, u8 reg, u8 data)
{
    ap3216c_write_regs(dev, reg, &data, 1);
}

void ap3216c_readdata(struct ap3216c_device *dev)
{
	unsigned char i =0;
    unsigned char buf[6];
	
	/* 循环读取所有传感器数据 */
    for(i = 0; i < 6; i++)	
    {
        buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);	
    }

    if(buf[0] & 0X80) { 	/* IR_OF位为1,则数据无效 */
		dev->ir = 0;
    } else { 				/* 读取IR传感器的数据   		*/
		dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03); 			
    }

	dev->als = ((unsigned short)buf[3] << 8) | buf[2];	/* 读取ALS传感器的数据 			 */  
	
    if(buf[4] & 0x40) {	/* IR_OF位为1,则数据无效 			*/
		dev->ps = 0;    													
    } else { 				/* 读取PS传感器的数据    */
		dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F); 
    }
}

/*------------------ 设备操作函数 ----------------------*/
//chr open
static int ap3216c_open(struct inode* inode, struct file* filp)
{
    /* 私有数据 */
    filp->private_data = &ap3216c_dev;

    //初始化ap3216c设备
    //复位
    ap3216c_write_reg(&ap3216c_dev, AP3216C_SYSTEMCONG, 0x04);
    mdelay(50);
    //上电
    ap3216c_write_reg(&ap3216c_dev, AP3216C_SYSTEMCONG, 0x00);
    mdelay(50);
    //ALS and PS+IR functions active
    ap3216c_write_reg(&ap3216c_dev, AP3216C_SYSTEMCONG, 0x03);
    //ALS+PS+IR全部转换结束需要112.5ms
    mdelay(113);

    return 0;
}

//chr write
static ssize_t ap3216c_write(struct file* filp, const char __user* buf, size_t count, loff_t* ppos)
{
    return 0;
}

//chr read
static ssize_t ap3216c_read(struct file* filp, char __user* buf, size_t count, loff_t* ppos)
{
    short data[3];
	long err = 0;

	struct ap3216c_device *dev = (struct ap3216c_device *)filp->private_data;
	
	ap3216c_readdata(dev);

	data[0] = dev->ir;
	data[1] = dev->als;
	data[2] = dev->ps;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;

}

//close chr dev
static int ap3216c_close(struct inode* inode, struct file* filp)
{
    return 0;
}

/* 设备操作函数集 */
static struct file_operations ap3216c_fops = {
    .owner    =  THIS_MODULE,
    .open     =  ap3216c_open,
    .read     =  ap3216c_read,
    .write    =  ap3216c_write,
    .release  =  ap3216c_close,
};

#if (IS_USE_MISC)
struct miscdevice ap3216c_misc = {
    .minor = 255,
    .name = "hao_ap3216c",
    .fops = &ap3216c_fops,
};
#endif

int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *dev)
{
    int err = 0;
    printk("ap3216c probe!\r\n");

#if (IS_USE_MISC == 0)
    /* 1、创建设备号 */
    if (ap3216c_dev.major) {  /* 定义了设备号 */
        ap3216c_dev.devid = MKDEV(ap3216c_dev.major, ap3216c_dev.minor);  //得到设备号
        //自己提供
        err = register_chrdev_region(ap3216c_dev.devid, 1, ap3216c_dev.name);
    } else {   /* 没有定义设备号 */
        err = alloc_chrdev_region(&ap3216c_dev.devid, 0, 1, ap3216c_dev.name);  /* 申请设备号，次设备号为:0 */
        ap3216c_dev.major = MAJOR(ap3216c_dev.devid);      /* 获取分配号的主设备号 */
        ap3216c_dev.minor = MINOR(ap3216c_dev.devid);      /* 获取分配号的次设备号 */
    }

    if (err < 0) {
        printk("ap3216c_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", ap3216c_dev.major, ap3216c_dev.minor);

    /* 2、初始化cdev */
    ap3216c_dev.cdev.owner = THIS_MODULE;
    cdev_init(&ap3216c_dev.cdev, &ap3216c_fops);

    /* 3、添加一个cdev */
    err = cdev_add(&ap3216c_dev.cdev, ap3216c_dev.devid, 1);
    if (err < 0) {
        goto fail_cdev;
    }

    /* 4、创建类 */
    ap3216c_dev.class = class_create(THIS_MODULE, ap3216c_dev.name);
    if (IS_ERR(ap3216c_dev.class)) {
        err = PTR_ERR(ap3216c_dev.class);
        goto fail_class;
    }

    /* 5、创建设备 */
    ap3216c_dev.device = device_create(ap3216c_dev.class, NULL, ap3216c_dev.devid, NULL, ap3216c_dev.name);
    if (IS_ERR(ap3216c_dev.device)) {
        err = PTR_ERR(ap3216c_dev.device);
        goto fail_device;
    }
#else
    misc_register(&ap3216c_misc);

#endif
    //得到 struct i2c_adapter
    ap3216c_dev.private_data = client;

    return 0;

#if (IS_USE_MISC == 0)
fail_device:
    class_destroy(ap3216c_dev.class);
fail_class:
    cdev_del(&ap3216c_dev.cdev);
fail_cdev:
    unregister_chrdev_region(ap3216c_dev.devid, 1);
fail_devid:
    return err;
#endif
}

int ap3216c_remove(struct i2c_client *client)
{
    printk("ap3216c remove!\r\n");

#if (IS_USE_MISC == 0)
     /* 删除cdev */
    cdev_del(&ap3216c_dev.cdev);

    /* 删除设备 */
    device_destroy(ap3216c_dev.class, ap3216c_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(ap3216c_dev.devid, 1);

    /* 删除类 */
    class_destroy(ap3216c_dev.class);
#else
    misc_deregister(&ap3216c_misc);
#endif

    return 0;
}

struct i2c_device_id ap3216c_device_id = {
    .name = "lsc,ap3216c",
};

//设备树匹配
struct of_device_id ap3216c_of_device[] = {
    { .compatible =  "lsc,ap3216c", },
    {  }
};

static struct i2c_driver ap3216c_driver = {
    //无设备树匹配
    .id_table = &ap3216c_device_id,
    //有设备树匹配
    .driver = {
        .owner = THIS_MODULE,
        .name = "ap3216c",
        .of_match_table = ap3216c_of_device,
    },
    .probe = ap3216c_probe,
    .remove = ap3216c_remove,
};

/* 驱动入口、出口 */
static int __init ap3216c_init(void)
{
    /* 注册iic总线 */
    return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216c_exit(void)
{
    i2c_del_driver(&ap3216c_driver);
}

/* 加载、注销 */
module_init(ap3216c_init);
module_exit(ap3216c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");