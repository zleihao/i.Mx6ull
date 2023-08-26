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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>

/* ioctl 命令 */
#define CLOSE_CMD         _IO(0xef, 1)        //关闭
#define OPEN_CMD          _IO(0xef, 2)        //打开
#define SET_PERIOD_CMD    _IOW(0xef, 3, int)  //设置周期数

struct timer_device {
    struct cdev    cdev;
    struct class*   class;    /* 类 */
    struct device*  device;   /* 设备 */
    dev_t          devid;     /* 设备号 */
    int            major;     /* 主设备号 */
    int            minor;     /* 次设备号 */
    char*           name;     /* 设备名称 */
    struct device_node    *nd; /* 设备树节点 */
    struct timer_list timer;
    int time_period;
    spinlock_t lock;  /* 自旋锁 */
    int led_gpio;
};
struct timer_device timer_dev = {
    .major = 0,
    .name = "timer_dev",  /* 驱动设备名称 */
    .time_period = 500,   /* 时钟周期500ms */
};

/*------------------ 设备操作函数 ----------------------*/
//timer open
static int timer_open(struct inode* inode, struct file* filp)
{
    int err = 0;
    /* 私有数据 */
    filp->private_data = &timer_dev;

    return err;
}

//timer write
static long timer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0, period;
    unsigned long flag;
    struct timer_device *dev = (struct timer_device*)file->private_data;

	switch (cmd) {
        case CLOSE_CMD:
            del_timer_sync(&dev->timer);
            break;
        case OPEN_CMD:
            spin_lock_irqsave(&dev->lock, flag);
            period = dev->time_period;
            spin_unlock_irqrestore(&dev->lock, flag);
            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(period));
            break;
        case SET_PERIOD_CMD:
            spin_lock_irqsave(&dev->lock, flag);
            dev->time_period = arg;
            spin_unlock_irqrestore(&dev->lock, flag);

            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
            break;
        default:
            ret = -1;
	}
	return ret;
}

//timer read
static ssize_t timer_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    int err = 0;
    struct timer_device* dev = (struct timer_device*)file->private_data;

    return err;
}

//close timer dev
static int timer_close(struct inode* inode, struct file* filp)
{
    return 0;
}

/* 设备操作函数集 */
static struct file_operations timer_fops = {
    .owner    =  THIS_MODULE,
    .open     =  timer_open,
    .read     =  timer_read,
    .unlocked_ioctl = timer_ioctl,
    .release  =  timer_close,
};

static void timer_func(unsigned long arg) 
{
    // struct timer_device *dev = (struct timer_device*)arg;
    int period = 0;
    unsigned long flag;

    static int state = 1;

    state = !state;

    gpio_set_value(timer_dev.led_gpio, state);

    /* 加锁 */
    spin_lock_irqsave(&timer_dev.lock, flag);
    period = timer_dev.time_period;
    /* 解锁 */
    spin_unlock_irqrestore(&timer_dev.lock, flag);

    mod_timer(&timer_dev.timer, jiffies + msecs_to_jiffies(period));
}

/* led_gpio init */
static int led_gpio_init(struct timer_device *dev)
{
    int err = 0;

    /* 获取节点 */
    dev->nd = of_find_node_by_path("/gpio_led");
    if (dev->nd == NULL) {
        err = -EINVAL;
        goto fail_find_node;
    }


    /* 获取gpio */
    dev->led_gpio = of_get_named_gpio(dev->nd, "led-gpios", 0);
    if (dev->led_gpio < 0) {
        err = -EINVAL;
        goto fail_find_node;
    }

    /* 设置gpio方向 */
    err = gpio_direction_output(dev->led_gpio, 1); 
    if (err < 0) {
        err = -EINVAL;
        goto fail_find_node;
    }

    return 0;

fail_find_node:
    return err;
}

/* 设备入口 */
static int __init timer_dev_init(void)
{
    int err = 0;

    /* 初始化自旋锁 */
    spin_lock_init(&timer_dev.lock);

//-------------------------------------------------------------------
//------------------------ 注册字符设备驱动 ----------------------------
//-------------------------------------------------------------------
    /* 1、创建设备号 */
    if (timer_dev.major) {  /* 定义了设备号 */
        timer_dev.devid = MKDEV(timer_dev.major, timer_dev.minor);  //得到设备号
        //自己提供
        err = register_chrdev_region(timer_dev.devid, 1, timer_dev.name);
    } else {   /* 没有定义设备号 */
        err = alloc_chrdev_region(&timer_dev.devid, 0, 1, timer_dev.name);  /* 申请设备号，次设备号为:0 */
        timer_dev.major = MAJOR(timer_dev.devid);      /* 获取分配号的主设备号 */
        timer_dev.minor = MINOR(timer_dev.devid);      /* 获取分配号的次设备号 */
    }

    if (err < 0) {
        printk("timer_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", timer_dev.major, timer_dev.minor);

    /* 2、初始化cdev */
    timer_dev.cdev.owner = THIS_MODULE;
    cdev_init(&timer_dev.cdev, &timer_fops);

    /* 3、添加一个cdev */
    err = cdev_add(&timer_dev.cdev, timer_dev.devid, 1);
    if (err < 0) {
        goto fail_cdev;
    }

    /* 4、创建类 */
    timer_dev.class = class_create(THIS_MODULE, timer_dev.name);
    if (IS_ERR(timer_dev.class)) {
        err = PTR_ERR(timer_dev.class);
        goto fail_class;
    }

    /* 5、创建设备 */
    timer_dev.device = device_create(timer_dev.class, NULL, timer_dev.devid, NULL, timer_dev.name);
    if (IS_ERR(timer_dev.device)) {
        err = PTR_ERR(timer_dev.device);
        goto fail_device;
    }
    
    /* 初始化led */
    err = led_gpio_init(&timer_dev);
    if (err < 0) {
        goto fail_led;
    }

    /* 初始化定时器，并不打开 */
    init_timer(&timer_dev.timer);
    timer_dev.timer.function = timer_func;

    return 0;

fail_led:
    del_timer(&timer_dev.timer);
    device_destroy(timer_dev.class, timer_dev.devid);
fail_device:
    class_destroy(timer_dev.class);
fail_class:
    cdev_del(&timer_dev.cdev);
fail_cdev:
    unregister_chrdev_region(timer_dev.devid, 1);
fail_devid:
    return err;
}

static void __exit timer_dev_exit(void)
{
    gpio_set_value(timer_dev.led_gpio, 1);
    del_timer(&timer_dev.timer);
    /* 删除cdev */
    cdev_del(&timer_dev.cdev);

    /* 删除设备 */
    device_destroy(timer_dev.class, timer_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(timer_dev.devid, 1);

    /* 删除类 */
    class_destroy(timer_dev.class);
}

/* 驱动挂载与卸载 */
module_init(timer_dev_init);
module_exit(timer_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");