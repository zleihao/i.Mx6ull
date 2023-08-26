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

#define LED_OFF    0  //关灯
#define LED_ON     1  //开灯

/* led 寄存器物理地址对应的虚拟地址 */
static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/* dts设备结构体 */
struct dts_dev_info {
    dev_t devid;      /* 设备号 */
    struct cdev cdev;
    struct class *class;
    struct device *device;  /*设备*/
    int major;        /* 主设备号 */
    int minor;        /* 子设备号 */
    char *name;       /* 设备名字 */
    struct device_node *nd; /* 设备树节点 */
};

struct dts_dev_info dts_dev = {
    .major = 0,
    .name = "dts_led"
};

static void led_switch(u8 state)
{
	u32 val = 0;

	if (state == LED_OFF) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);
		val |= (1 << 3);
		writel(val, GPIO1_DR);
	} else {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);
		writel(val, GPIO1_DR);
	}
}

//led open
static int dts_led_open(struct inode *inode, struct file *filp)
{
    int ret = 0;

    /* 私有数据 */
    filp->private_data = &dts_dev;

    return ret;
}

//led write
static ssize_t dts_led_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
	u8 data_buf[1];

	if (count > 1) {
		printk("write data too long...\r\n");
		return -EPERM;
	}
	ret = copy_from_user(data_buf, buf, count);
	if (ret < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	/* 判断是开灯还是关灯 */
	switch (data_buf[0])
	{
		case LED_OFF:
			//设置IO值，置1
			led_switch(LED_OFF);
			break;
		case LED_ON:
		    //设置IO值，清0
			led_switch(LED_ON);
			break;
		default:
			printk("using error!\r\n");
			break;
	}

	return 0;
}

//close led dev
static int dts_led_close(struct inode *inode, struct file *filp)
{
    struct dts_dev_info __attribute((unused)) *dev = (struct dts_dev_info *)filp->private_data;

    return 0;
}

const struct file_operations dts_fops = {
    .owner = THIS_MODULE,
    .open = dts_led_open,
    .write = dts_led_write,
    .release = dts_led_close,
};

/* 设备入口、出口 */
static int __init dts_led_init(void)
{
    int ret = 0;
    const char *str;
    u32 val;

    /* 获取设备号 */
    dts_dev.major = 0;
    if (dts_dev.major) {
        dts_dev.devid = MKDEV(dts_dev.major, dts_dev.minor);
        /* 已经提供 */
        ret = register_chrdev_region(dts_dev.devid, 0, dts_dev.name);
    } else {
        ret = alloc_chrdev_region(&dts_dev.devid, 0, 1, dts_dev.name);
    }

    if (ret < 0) {
        goto fail_register;
    } else {
        dts_dev.major = MAJOR(dts_dev.devid);
        dts_dev.minor = MINOR(dts_dev.devid);
        printk("dts led major = %d, minor = %d\r\n", dts_dev.major, dts_dev.minor);
    }

    /* 初始化cdev */
    dts_dev.cdev.owner = THIS_MODULE;
    cdev_init(&dts_dev.cdev, &dts_fops);
    /* 添加cdev */
    ret = cdev_add(&dts_dev.cdev, dts_dev.devid, 1);
    if (ret < 0) {
        goto fail_cdev_add;
    }

    /* 初始化类 */
    dts_dev.class = class_create(THIS_MODULE, dts_dev.name);
    if (IS_ERR(dts_dev.class)) {
        ret = PTR_ERR(dts_dev.class);
        goto fail_class_create;
    }

    /* 添加设备 */
    dts_dev.device = device_create(dts_dev.class, NULL, dts_dev.devid, NULL, dts_dev.name);
    if (IS_ERR(dts_dev.device)) {
        ret = PTR_ERR(dts_dev.device);
        goto fail_device_create;
    }

    /* 获取设备树节点 */
    dts_dev.nd = of_find_node_by_path("/zlh_led");
    if (dts_dev.nd == NULL) {
        ret = -EINVAL;
        goto fail_find_nd;
    }

    /* 得到status */
    ret = of_property_read_string(dts_dev.nd, "status", &str);
    if (ret < 0) {
        goto fail_find_rs;
    } else {
        /* 成功获取到status */
        printk("status = %s\r\n", str);
    }

#if 0
    /* 获取reg的值 */
    ret = of_property_read_u32_array(dts_dev.nd, "reg", reg_data, 10);
    if (ret < 0) {
        goto fail_find_rs;
    } else {
        printk("reg data:\r\n");
        for (i = 0; i < 10; i++) {
            printk("%#x ", reg_data[i]);
        }
        printk("\r\n");
    }

    /* LED */
    /* 1.初始化LED灯时钟与GPIO，虚拟地址映射 */
	CCM_CCGR1 = ioremap(reg_data[0], reg_data[1]);
	SW_MUX_GPIO1_IO03 = ioremap(reg_data[2], reg_data[3]);
	SW_PAD_GPIO1_IO03 = ioremap(reg_data[4], reg_data[5]);
	GPIO1_DR = ioremap(reg_data[6], reg_data[7]);
	GPIO1_GDIR = ioremap(reg_data[8], reg_data[9]);
#endif

    /* 1.初始化LED灯时钟与GPIO，虚拟地址映射 */
	CCM_CCGR1 = of_iomap(dts_dev.nd, 0);
	SW_MUX_GPIO1_IO03 = of_iomap(dts_dev.nd, 1);
	SW_PAD_GPIO1_IO03 = of_iomap(dts_dev.nd, 2);
	GPIO1_DR = of_iomap(dts_dev.nd, 3);
	GPIO1_GDIR = of_iomap(dts_dev.nd, 4);

    /* 初始化时钟 */
	val = readl(CCM_CCGR1);
	val &= ~(3 << 26);
	val |= 3 << 26;
	writel(val, CCM_CCGR1);

	//设置IO复用
	writel(0x05, SW_MUX_GPIO1_IO03);
	//设置电气属性
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	//设置IO方向
	val = readl(GPIO1_GDIR);
	val |= 1 << 3;
	writel(val, GPIO1_GDIR);

    /* 关灯 */
    led_switch(LED_OFF);

    return 0;

fail_find_rs:
fail_find_nd:
    device_destroy(dts_dev.class, dts_dev.devid);
fail_device_create:
    class_destroy(dts_dev.class);
fail_class_create:
    cdev_del(&dts_dev.cdev);
fail_cdev_add:
    unregister_chrdev_region(dts_dev.devid, 1);
fail_register:
    return ret;
}

static void __exit dts_led_exit(void)
{
    /* 关灯 */
    led_switch(LED_OFF);
 
    /* 取消地址映射 */
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    /* 删除cdev */
    cdev_del(&dts_dev.cdev);

    /* 删除设备 */
    device_destroy(dts_dev.class, dts_dev.devid);
    
    /* 删除设备号 */
    unregister_chrdev_region(dts_dev.devid, 1);

    /* 删除类 */
    class_destroy(dts_dev.class);
}

/* 设备注册与注销 */
module_init(dts_led_init);
module_exit(dts_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhaoleihao");
