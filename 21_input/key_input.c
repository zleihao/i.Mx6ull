#include "asm/atomic.h"
#include "linux/interrupt.h"
#include "linux/irqreturn.h"
#include "linux/mod_devicetable.h"
#include "linux/types.h"
#include "uapi/linux/input.h"
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
#include <linux/input.h>

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

struct key_input_device {
    char          name[20];     /* 设备名称 */
    struct device_node *nd;        /* 设备树节点 */
    struct irq_key_des irq_key[KEY_NUM];
    /* 定时器 */
    struct timer_list timer;

    struct input_dev *inputdev;
};
struct key_input_device key_input_dev = {
    .name = "key_input",  /* 驱动设备名称 */
};

/* 中断处理函数 */
static irqreturn_t key0_handler(int irq, void *devid) 
{
    struct key_input_device *dev = (struct key_input_device *)devid;
    

    dev->timer.data = (volatile unsigned long)dev;
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(20));

    
    return 0;
}

static void timer_func(unsigned long arg) 
{
    int value = 0;
    struct key_input_device *dev = (struct key_input_device *)arg;
    
    value = gpio_get_value(dev->irq_key[0].key_gpio);

    if (value == 0) {
        /* 上报数据 */
        input_event(dev->inputdev, EV_KEY, KEY_0, 1);
        /* 同步 */
        input_sync(dev->inputdev);
    } else if (value == 1) {
        /* 上报数据 */
        input_event(dev->inputdev, EV_KEY, KEY_0, 0);
        /* 同步 */
        input_sync(dev->inputdev);
    }
}

static int key_init(struct key_input_device *dev) 
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
                          &key_input_dev
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
int __init key_input_dev_init(void)
{
    int err = 0;

    err = key_init(&key_input_dev);
    if (err < 0) {
        goto fail_key_init;
    }

    /* 初始化input */
    key_input_dev.inputdev = input_allocate_device();
    if (key_input_dev.inputdev == NULL) {
        err = -EINVAL;
        goto fail_key_init;
    }

    __set_bit(EV_KEY, key_input_dev.inputdev->evbit);  /* 按键事件 */
    __set_bit(EV_REP, key_input_dev.inputdev->evbit);  /* 重复事件 */
    __set_bit(KEY_0, key_input_dev.inputdev->keybit);

    /* 注册input */
    err = input_register_device(key_input_dev.inputdev);
    if (err) {
		printk("Can't register input device: %d\n", err);
		goto fail_input_register;
	}

    return 0;

fail_input_register:
    input_free_device(key_input_dev.inputdev);
fail_key_init:
    return err;
}

void __exit key_input_dev_exit(void)
{
    int i = 0;
    /* 释放IO中断 */
    for (i = 0; i < KEY_NUM; i++) {
        free_irq(key_input_dev.irq_key[i].irq_num, &key_input_dev);
    }

    /* 释放IO资源 */
    for (i = 0; i < KEY_NUM; i++) {
        gpio_free(key_input_dev.irq_key[i].key_gpio);
    }

    /* 删除定时器 */
    del_timer_sync(&key_input_dev.timer);

    input_unregister_device(key_input_dev.inputdev);

    input_free_device(key_input_dev.inputdev);
}

/* 驱动挂载与卸载 */
module_init(key_input_dev_init);
module_exit(key_input_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");