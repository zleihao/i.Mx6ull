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

//设备名字
#define CHR_LED_NAME "hao_led"

#define LED_OFF    0  //关灯
#define LED_ON     1  //开灯

/* led 寄存器物理地址 */
#define CCM_CCGR1_BASE          (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE  (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE  (0x020E02F4)
#define GPIO1_DR_BASE           (0x0209C000)
#define GPIO1_GDIR_BASE         (0x0209C004)

/* led 寄存器物理地址对应的虚拟地址 */
static void __iomem* CCM_CCGR1;
static void __iomem* SW_MUX_GPIO1_IO03;
static void __iomem* SW_PAD_GPIO1_IO03;
static void __iomem* GPIO1_DR;
static void __iomem* GPIO1_GDIR;

/* led设备结构体 */
struct chr_led_dev {
    struct cdev     cdev;
    struct class*   class;
    struct device*  device;   /* 设备 */
    dev_t           devid;   /* 设备号 */
    int             major;   /* 主设备号 */
    int             minor;   /* 次设备号 */
};
static struct chr_led_dev led_dev;

/* 文件操作集合 */
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
};

/* 设备入口 */
static int __init chr_led_init(void)
{
    int ret = 0;
    printk("chr_led_init\r\n");

    /* 获取设备号 */
    if (led_dev.major) {
        led_dev.devid = MKDEV(led_dev.major, led_dev.minor);  //得到设备号
        //自己提供
        ret = register_chrdev_region(led_dev.devid, 1, CHR_LED_NAME);
    } else {
        //向内核申请
        ret = alloc_chrdev_region(&led_dev.devid, 100, 1, CHR_LED_NAME);
        /* 得到主设备号、次设备号 */
        led_dev.major = MAJOR(led_dev.devid);
        led_dev.minor = MINOR(led_dev.devid);
    }

    if (ret < 0) {
        printk("led_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", led_dev.major, led_dev.minor);

    led_dev.cdev.owner = THIS_MODULE;
    /* 初始化cdev */
    cdev_init(&led_dev.cdev, &led_fops);
    /* 添加cdev */
    ret = cdev_add(&led_dev.cdev, led_dev.devid, 1);
    if (ret < 0) {
        goto fail_cdev;
    }

    /* 创建类 */
    led_dev.class = class_create(THIS_MODULE, CHR_LED_NAME);
    if (IS_ERR(led_dev.class)) {
        ret = PTR_ERR(led_dev.class);
        goto fail_class;
    }

    /* 添加设备到class */
    led_dev.device = device_create(led_dev.class, NULL, led_dev.devid, NULL, CHR_LED_NAME);
    if (IS_ERR(led_dev.device)) {
        ret = PTR_ERR(led_dev.device);
        goto fail_device;
    }

    return 0;

fail_device:
    class_destroy(led_dev.class);
fail_class:
    cdev_del(&led_dev.cdev);
fail_cdev:
    unregister_chrdev_region(led_dev.devid, 1);
fail_devid:
    return ret;
}

static void __exit chr_led_exit(void)
{
    printk("chr_led_exit\r\n");

    /* 删除cdev */
    cdev_del(&led_dev.cdev);

    /* 删除设备 */
    device_destroy(led_dev.class, led_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(led_dev.devid, 1);

    /* 删除类 */
    class_destroy(led_dev.class);
}

/* 设备注册与注销 */
module_init(chr_led_init);
module_exit(chr_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");