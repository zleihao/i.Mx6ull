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

#define KEY_VALUE   0xf0
#define INVA_VALUE  0x00

/* gpio驱动结构体 */
struct key_device {
    dev_t devid;
    struct device_node *nd;
    struct device *device;
    struct class *class;
    struct cdev cdev;
    int major;
    int minjor;
    int key_gpio;
    atomic_t key_value;
    char *name;
};
struct key_device key_dev = {
    .major = 0, /* 由内核申请 */
    .name = "key",
};

//key open
static int key_open(struct inode *inode, struct file *filp)
{
    int ret = 0;

    /* 私有数据 */
    filp->private_data = &key_dev;

    return ret;
}

//key write
static ssize_t key_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{


	return 0;
}

//key read
static ssize_t key_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    int err = 0, value = 0;
    struct key_device *dev = (struct key_device *)file->private_data;

    if (gpio_get_value(dev->key_gpio) == 0) {
        while (gpio_get_value(dev->key_gpio) == 0);
        atomic_set(&dev->key_value, KEY_VALUE);
    } else {
        atomic_set(&dev->key_value, INVA_VALUE);
    }

    value = atomic_read(&dev->key_value);

    err = copy_to_user(buf, &value, sizeof(value));

    return err;
}

//close key dev
static int key_close(struct inode *inode, struct file *filp)
{
    

    return 0;
}

struct file_operations gpio_key_fops = {
    .owner = THIS_MODULE, 
    .open = key_open,
    .write = key_write,
    .read = key_read,
    .release = key_close,
};

/* key io 初始化 */
static int keyio_init(struct key_device *dev)
{
    int err = 0;

    /* 获取key设备节点 */
    dev->nd = of_find_node_by_path("/key");
    if (dev->nd == NULL) {
        err = -EINVAL;
        printk("Can't find node of key!\r\n");
        goto fail_find_nd;
    }

    /* 获取gpio */
    dev->key_gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
    if (dev->key_gpio < 0) {
        err = -EINVAL;
        printk("Can't find gpio of key!\r\n");
        goto fail_find_gpio;
    }
    printk("key gpio num is %d\r\n", dev->key_gpio);

    /* 申请GPIO */
    err = gpio_request(dev->key_gpio, "key-gpio");
    if (err < 0) {
        err = -EINVAL;
        printk("Request gpio of key fail!\r\n");
        goto fail_find_gpio;
    }
    printk("Request gpio of key success\r\n");

    /* 设置key gpio 方向 */
    err = gpio_direction_input(dev->key_gpio);
    if (err < 0) {
        goto fail_request_gpio;
    }

    return 0;

fail_request_gpio:
    gpio_free(dev->key_gpio);
fail_find_gpio:
fail_find_nd:
    return err;
}

/* 入口与出口 */
static int __init gpio_key_init(void)
{
    int ret = 0;

    /* 原子初始化 */
    atomic_set(&key_dev.key_value, INVA_VALUE);

    if (key_dev.major) {
        key_dev.devid = MKDEV(key_dev.major, key_dev.minjor);
        ret = register_chrdev_region(key_dev.devid, 1, key_dev.name);
    } else {
        ret = alloc_chrdev_region(&key_dev.devid, 0, 1, key_dev.name);
    }
    if (ret < 0) {
        printk(KERN_ERR "alloc_chrdev_region faikey\n");
        goto fail_dev;
    } else {
        key_dev.major = MAJOR(key_dev.devid);
        key_dev.minjor = MINOR(key_dev.devid);
        printk("major = %d, minjor = %d\r\n", key_dev.major, key_dev.minjor);
    }

    // cdev
    cdev_init(&key_dev.cdev, &gpio_key_fops);
    key_dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&key_dev.cdev, key_dev.devid, 1);
    if (ret < 0) {
        printk(KERN_ERR "cdev_add faikey\n");
        goto fail_chrdev;
    }

    //class
    key_dev.class = class_create(THIS_MODULE, key_dev.name);
    if (IS_ERR(key_dev.class)) {
        ret = PTR_ERR(key_dev.class);
        goto fail_class;
    }

    //device
    key_dev.device = device_create(key_dev.class, NULL, key_dev.devid, NULL, key_dev.name);
    if (IS_ERR(key_dev.device)) {
        ret = PTR_ERR(key_dev.device);
        goto fail_device;
    }

    ret = keyio_init(&key_dev);
    if (ret < 0) {
        goto fail_key_init;
    }

    return 0;

fail_key_init:
    device_destroy(key_dev.class, key_dev.devid);
fail_device:
    class_destroy(key_dev.class);
fail_class:
    cdev_del(&key_dev.cdev);
fail_chrdev:
    unregister_chrdev_region(key_dev.devid, 1);
fail_dev:
    return ret;
}

static void __exit gpio_key_exit(void)
{
    /* 释放gpio */
    gpio_free(key_dev.key_gpio);

    cdev_del(&key_dev.cdev);
    
    unregister_chrdev_region(key_dev.devid, 1);

    device_destroy(key_dev.class, key_dev.devid);

    class_destroy(key_dev.class);
}

/* 挂载与注销 */
module_init(gpio_key_init);
module_exit(gpio_key_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");
