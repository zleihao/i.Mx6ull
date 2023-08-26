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
#include <linux/of_address.h>
#include <linux/slab.h>

#if 0
    backlight {
		compatible = "pwm-backlight";
		pwms = <&pwm1 0 5000000>;
		brightness-levels = <0 4 8 16 32 64 128 255>;
		default-brightness-level = <6>;
		status = "okay";
	};

#endif

/* 入口、出口 */
static int __init dtsof_init(void)
{
    int ret = 0, i;
    struct device_node *backlight = NULL;
    struct property *com_pro = NULL;
    const char *str;
    u32 def_value = 0, size = 0;
    u32 *brival = NULL;

    /* 找到 backlight节点，路径：/backlight */
    backlight = of_find_node_by_path("/backlight");
    if (backlight == NULL) {
        ret = -EINVAL;
        goto fail_find_nd;
    }

    /* 得到 compatible值*/
    com_pro = of_find_property(backlight, "compatible", NULL);
    if (com_pro == NULL) {
        ret = -EINVAL;
        goto fail_find_property;
    } else {
        /* 成功获取到compatible */
        printk("/backlight::compatible = %s\r\n", (char *)com_pro->value);
    }

    /* 得到status的值 */
    ret = of_property_read_string(backlight, "status", &str);
    if (ret < 0) {
        goto fail_find_rs;
    } else {
        /* 成功获取到status */
        printk("/backlight::status = %s\r\n", str);
    }
    /* 获取default-brightness-level的值 */
    ret = of_property_read_u32(backlight, "default-brightness-level", &def_value);
    if (ret < 0) {
        goto fail_read_u32;
    } else {
        /* 成功获取到 default-brightness-level */
        printk("/backlight::default-brightness-level = %d\r\n", def_value);
    }

    /* 首先获取 brightness-levels 长度 */
    size = of_property_count_elems_of_size(backlight, "brightness-levels", sizeof(u32));
    if (size < 0) {
        ret = -EINVAL;
        goto fail_elems;
    } else {
        /* 成功获取到 brightness-levels 长度 */
        printk("/backlight::brightness-levels size = %d\r\n", size);
    }

    //申请内存
    brival = kmalloc(size * sizeof(u32), GFP_KERNEL);
    if (brival == NULL) {
        ret = -EINVAL;
        goto fail_kmalloc;
    }

    /* 读取 brightness-levels数据*/
    ret = of_property_read_u32_array(backlight, "brightness-levels", brival, size);
    if (ret < 0) {
        goto fail_read_u32arr;
    } else {
        for (i = 0; i < size; i++) {
            printk("brightness-levels[%d] = %d\r\n", i, brival[i]);
        }
    }
    kfree(brival);

    return 0;

fail_read_u32arr:
    kfree(brival);
fail_kmalloc:
fail_elems:
fail_read_u32:
fail_find_rs:
fail_find_property:
fail_find_nd:
    return ret;
}

static void __exit dtsof_exit(void)
{

}

/* 设备注册与注销 */
module_init(dtsof_init);
module_exit(dtsof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");