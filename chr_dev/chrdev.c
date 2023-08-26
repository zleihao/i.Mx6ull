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

struct chr_device {
    struct cdev    cdev;
    struct class   *class;    /* 类 */
    struct device  *device;   /* 设备 */
    dev_t          devid;     /* 设备号 */
    int            major;     /* 主设备号 */
    int            minor;     /* 次设备号 */
    char           *name;     /* 设备名称 */
};
struct chr_device chr_dev = {
    .major = 0, 
    .name = "chr_dev",  /* 驱动设备名称 */
};

/*------------------ 设备操作函数 ----------------------*/
//chr open
static int chr_open(struct inode *inode, struct file *filp)
{
    int err = 0;

    /* 私有数据 */
    filp->private_data = &chr_dev;

    return err;
}

//chr write
static ssize_t chr_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

//chr read
static ssize_t chr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    int err = 0;
    struct chr_device *dev = (struct chr_device *)file->private_data;

    return err;
}

//close chr dev
static int chr_close(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 设备操作函数集 */
static struct file_operations chr_fops = {
    .owner    =  THIS_MODULE,
    .open     =  chr_open,
    .read     =  chr_read,
    .write    =  chr_write,
    .release  =  chr_close,
};

/* 设备入口 */
static int __init chr_dev_init(void)
{
    int err = 0;

//-------------------------------------------------------------------
//------------------------ 注册字符设备驱动 ----------------------------
//-------------------------------------------------------------------
	/* 1、创建设备号 */
    if (chr_dev.major) {  /* 定义了设备号 */
        chr_dev.devid = MKDEV(chr_dev.major, chr_dev.minor);  //得到设备号
        //自己提供
        err = register_chrdev_region(chr_dev.devid, 1, chr_dev.name);
    } else {   /* 没有定义设备号 */
        err = alloc_chrdev_region(&chr_dev.devid, 0, 1, chr_dev.name);  /* 申请设备号，次设备号为:0 */
        chr_dev.major = MAJOR(chr_dev.devid);      /* 获取分配号的主设备号 */
        chr_dev.minor = MINOR(chr_dev.devid);      /* 获取分配号的次设备号 */
    }

    if (err < 0) {
        printk("chr_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", chr_dev.major, chr_dev.minor);

    /* 2、初始化cdev */
    chr_dev.cdev.owner = THIS_MODULE;
    cdev_init(&chr_dev.cdev, &chr_fops);

    /* 3、添加一个cdev */
    err = cdev_add(&chr_dev.cdev, chr_dev.devid, 1);
    if (err < 0) {
        goto fail_cdev;
    }

    /* 4、创建类 */
    chr_dev.class = class_create(THIS_MODULE, chr_dev.name);
    if (IS_ERR(chr_dev.class)) {
        err = PTR_ERR(chr_dev.class);
        goto fail_class;
    }

    /* 5、创建设备 */
    chr_dev.device = device_create(chr_dev.class, NULL, chr_dev.devid, NULL, chr_dev.name);
    if (IS_ERR(chr_dev.device)) {
        err = PTR_ERR(chr_dev.device);
        goto fail_device;
    }

    return 0;

fail_device:
    class_destroy(chr_dev.class);
fail_class:
    cdev_del(&chr_dev.cdev);
fail_cdev:
    unregister_chrdev_region(chr_dev.devid, 1);
fail_devid:
    return err;
}

static void __exit chr_dev_exit(void)
{
    /* 删除cdev */
    cdev_del(&chr_dev.cdev);

    /* 删除设备 */
    device_destroy(chr_dev.class, chr_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(chr_dev.devid, 1);

    /* 删除类 */
    class_destroy(chr_dev.class);
}

/* 驱动挂载与卸载 */
module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");