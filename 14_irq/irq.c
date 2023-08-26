#include "asm/atomic.h"
#include "linux/interrupt.h"
#include "linux/irqreturn.h"
#include "linux/types.h"
#include <asm-generic/errno-base.h>
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
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#define KEY_VALUE  0x01
#define KEY_INVA   0x0f

#define KEY_NUM 1

/* 按键结构体 */
struct irq_key_des {
    int key_gpio;
    int irq_num;
    unsigned int value;
    char name[10];
    irqreturn_t (*irq_handle_t)(int, void*);                        /* 中断处理函数 */
};

struct mx6ull_irq_device {
    struct cdev    cdev;
    struct class*   class;    /* 类 */
    struct device*  device;   /* 设备 */
    dev_t          devid;     /* 设备号 */
    int            major;     /* 主设备号 */
    int            minor;     /* 次设备号 */
    char          name[20];     /* 设备名称 */
    struct device_node *nd;        /* 设备树节点 */
    struct irq_key_des irq_key[KEY_NUM];
    /* 定时器 */
    struct timer_list timer;

    /* 按键值 */
    atomic_t key_value;
    /* 按键状态 */
    atomic_t key_state;
};
struct mx6ull_irq_device mx6ull_irq_dev = {
    .major = 0,
    .name = "mx6ull_irq_dev",  /* 驱动设备名称 */
};

/*------------------ 设备操作函数 ----------------------*/
//chr open
static int chr_open(struct inode* inode, struct file* filp)
{
    int err = 0;

    /* 私有数据 */
    filp->private_data = &mx6ull_irq_dev;

    return err;
}

//chr write
static ssize_t chr_write(struct file* filp, const char __user* buf, size_t count, loff_t* ppos)
{
    return 0;
}

//chr read
static ssize_t chr_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    int err = 0;
    struct mx6ull_irq_device* dev = (struct mx6ull_irq_device*)file->private_data;
    unsigned char key_value;
    unsigned char key_state;

    key_value = atomic_read(&dev->key_value);
    key_state = atomic_read(&dev->key_state);

    if (key_state) {  /* 表示按键按下 */
        key_value &= ~0x80;
        err = copy_to_user(buf, &key_value, sizeof(key_value));
        atomic_set(&dev->key_state, 0);
    } else {
        return -EINVAL;
    }

    return err;
}

//close chr dev
static int chr_close(struct inode* inode, struct file* filp)
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

/* 中断处理函数 */
static irqreturn_t key0_handler(int irq, void *devid) 
{
    struct mx6ull_irq_device *dev = (struct mx6ull_irq_device *)devid;
    

    dev->timer.data = (volatile unsigned long)dev;
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(20));

    
    return 0;
}

static void timer_func(unsigned long arg) 
{
    int value = 0;
    struct mx6ull_irq_device *dev = (struct mx6ull_irq_device *)arg;
    
    value = gpio_get_value(dev->irq_key[0].key_gpio);

    if (value == 0) {
        printk("key0 push!!!\r\n");
        atomic_set(&dev->key_value, dev->irq_key[0].value);  //得到对应按键的值
    } else if (value == 1) {
        printk("key0 release!!!\r\n");
        atomic_set(&dev->key_value, 0x80 | dev->irq_key[0].value);  //释放后，得到对应按键的值高位置1
        atomic_set(&dev->key_state, 1);   //按键释放
    }
}

static int key_init(struct mx6ull_irq_device *dev) 
{
    int err = 0;
    int i = 0;

    /* 获取key设备树节点 */
    dev->nd = of_find_node_by_path("/key");
    if (dev->nd == NULL) {
        printk("Can't find node of key!\r\n");
        err = -EINVAL;
        goto fail_find_nd;
    }

    for (i = 0; i < KEY_NUM; i++) {
        /* 得到IO编号 */
        dev->irq_key[i].key_gpio = of_get_named_gpio(dev->nd, "key-gpios", i);
        if (dev->irq_key[i].key_gpio < 0) {
            printk("Can't find gpio of key!\r\n");
            err = -EINVAL;
            goto fail_find_nd;
        }
        
        memset(dev->irq_key[i].name, 0, sizeof(dev->irq_key[i].name));
        sprintf(dev->irq_key[i].name, "KEY%d", i);
        /* 申请IO资源 */
        err = gpio_request(dev->irq_key[i].key_gpio, dev->irq_key[i].name);
        if (err) {
            printk("Can't get KEY gpio: %d\n", dev->irq_key[i].key_gpio);
            goto fail_find_nd;
        }
    }

    for (i = 0; i < KEY_NUM; i++) {
         /* 设置IO方向 */
        err = gpio_direction_input(dev->irq_key[i].key_gpio);
        if (err) {
            printk("Set gpio of key direction err!\r\n");
            goto fail_set_direction;
        }

        /* 获取中断号 */
        dev->irq_key[i].irq_num = irq_of_parse_and_map(dev->nd, i);
    }

    dev->irq_key[0].irq_handle_t = key0_handler;
    dev->irq_key[0].value = KEY_VALUE;
    /* 按键中断初始化 */
    for (i = 0; i < KEY_NUM; i++) {
        err = request_irq(dev->irq_key[i].irq_num, dev->irq_key[i].irq_handle_t, 
                          IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                          dev->irq_key[i].name, 
                          &mx6ull_irq_dev
                         );
        if (err) {
            goto fail_set_direction;
        }
    }

    /* 初始化定时器 */
    init_timer(&dev->timer);

    /* 回调函数 */
    dev->timer.function = timer_func;


    return 0;

fail_set_direction:
    for (i = 0; i < KEY_NUM; i++) {
        gpio_free(dev->irq_key[i].key_gpio);
    }
fail_find_nd:
    return err;
}

/* 设备入口 */
int __init mx6ull_irq_dev_init(void)
{
    int err = 0;
//-------------------------------------------------------------------
//------------------------ 注册字符设备驱动 ----------------------------
//-------------------------------------------------------------------
    /* 1、创建设备号 */
    if (mx6ull_irq_dev.major) {  /* 定义了设备号 */
        mx6ull_irq_dev.devid = MKDEV(mx6ull_irq_dev.major, mx6ull_irq_dev.minor);  //得到设备号
        //自己提供
        err = register_chrdev_region(mx6ull_irq_dev.devid, 1, mx6ull_irq_dev.name);
    } else {   /* 没有定义设备号 */
        err = alloc_chrdev_region(&mx6ull_irq_dev.devid, 0, 1, mx6ull_irq_dev.name);  /* 申请设备号，次设备号为:0 */
        mx6ull_irq_dev.major = MAJOR(mx6ull_irq_dev.devid);      /* 获取分配号的主设备号 */
        mx6ull_irq_dev.minor = MINOR(mx6ull_irq_dev.devid);      /* 获取分配号的次设备号 */
    }

    if (err < 0) {
        printk("mx6ull_irq_dev register fail!\r\n");
        goto fail_devid;
    }
    printk("major:%d,minor:%d\r\n", mx6ull_irq_dev.major, mx6ull_irq_dev.minor);

    /* 2、初始化cdev */
    mx6ull_irq_dev.cdev.owner = THIS_MODULE;
    cdev_init(&mx6ull_irq_dev.cdev, &chr_fops);

    /* 3、添加一个cdev */
    err = cdev_add(&mx6ull_irq_dev.cdev, mx6ull_irq_dev.devid, 1);
    if (err < 0) {
        goto fail_cdev;
    }

    /* 4、创建类 */
    mx6ull_irq_dev.class = class_create(THIS_MODULE, mx6ull_irq_dev.name);
    if (IS_ERR(mx6ull_irq_dev.class)) {
        err = PTR_ERR(mx6ull_irq_dev.class);
        goto fail_class;
    }

    /* 5、创建设备 */
    mx6ull_irq_dev.device = device_create(mx6ull_irq_dev.class, NULL, mx6ull_irq_dev.devid, NULL, mx6ull_irq_dev.name);
    if (IS_ERR(mx6ull_irq_dev.device)) {
        err = PTR_ERR(mx6ull_irq_dev.device);
        goto fail_device;
    }

    err = key_init(&mx6ull_irq_dev);
    if (err < 0) {
        goto fail_key_init;
    }

    /* 初始化原子变量 */
    atomic_set(&mx6ull_irq_dev.key_value, KEY_INVA);
    atomic_set(&mx6ull_irq_dev.key_state, 0);

    return 0;

fail_key_init:
    device_destroy(mx6ull_irq_dev.class, mx6ull_irq_dev.devid);
fail_device:
    class_destroy(mx6ull_irq_dev.class);
fail_class:
    cdev_del(&mx6ull_irq_dev.cdev);
fail_cdev:
    unregister_chrdev_region(mx6ull_irq_dev.devid, 1);
fail_devid:
    return err;
}

void __exit mx6ull_irq_dev_exit(void)
{
    int i = 0;
    /* 释放IO中断 */
    for (i = 0; i < KEY_NUM; i++) {
        free_irq(mx6ull_irq_dev.irq_key[i].irq_num, &mx6ull_irq_dev);
    }

    /* 释放IO资源 */
    for (i = 0; i < KEY_NUM; i++) {
        gpio_free(mx6ull_irq_dev.irq_key[i].key_gpio);
    }

    /* 删除定时器 */
    del_timer_sync(&mx6ull_irq_dev.timer);

    /* 删除cdev */
    cdev_del(&mx6ull_irq_dev.cdev);

    /* 删除设备 */
    device_destroy(mx6ull_irq_dev.class, mx6ull_irq_dev.devid);

    /* 注销设备号 */
    unregister_chrdev_region(mx6ull_irq_dev.devid, 1);

    /* 删除类 */
    class_destroy(mx6ull_irq_dev.class);
}

/* 驱动挂载与卸载 */
module_init(mx6ull_irq_dev_init);
module_exit(mx6ull_irq_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");