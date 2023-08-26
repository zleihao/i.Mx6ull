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

//主设备号
#define LED_MAJOR   200
//设备名字
#define NEW_CHR_LED_NAME "hao_led"

#define LED_OFF    0  //关灯
#define LED_ON     1  //开灯

/* led 寄存器物理地址 */
#define CCM_CCGR1_BASE          (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE  (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE  (0x020E02F4)
#define GPIO1_DR_BASE           (0x0209C000)
#define GPIO1_GDIR_BASE         (0x0209C004)

/* led 寄存器物理地址对应的虚拟地址 */
static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/* LED设备结构体 */
struct new_chr_led_dev {
	struct cdev cdev;
	struct class *class;   /* 类 */
	struct device *device; /* 设备 */
	dev_t devid;           /* 设备号 */
	int   major;           /* 主设备号 */
	int   minor;           /* 次设备号 */
};
struct new_chr_led_dev new_chr_led;

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

//led write
static ssize_t new_chr_led_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
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

//led open
static int new_chr_led_open(struct inode *inode, struct file *filp)
{
	/* 设置私有数据 */
	filp->private_data = &new_chr_led;
	return 0;
}

//close led dev
static int new_chr_led_close(struct inode *inode, struct file *filp)
{
	struct new_chr_led_dev *led_dev = filp->private_data;

	led_dev = NULL; 
	return 0;
}

static const struct file_operations new_chr_led_fops = {
	.owner = THIS_MODULE,
	.open = new_chr_led_open,
	.write = new_chr_led_write,
	.release = new_chr_led_close,
};

/* 入口 */
static int __init new_chr_led_init(void)
{
	int ret = 0;
	u32 val = 0;
	
	printk("new chr led init!\r\n");

	/* 1.初始化LED灯时钟与GPIO，虚拟地址映射 */
	CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
	SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
	GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
	GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

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

	//关灯
	led_switch(LED_OFF);

	/* 获取字符设备号 */
	if (new_chr_led.major) {
		new_chr_led.devid = MKDEV(new_chr_led.major, 0);
		ret = register_chrdev_region(new_chr_led.devid, 1, NEW_CHR_LED_NAME);
	} else { /* 没有给定设备号 */
        ret = alloc_chrdev_region(&new_chr_led.devid, 0, 1, NEW_CHR_LED_NAME);
		new_chr_led.major = MAJOR(new_chr_led.devid);
		new_chr_led.minor = MINOR(new_chr_led.devid);
	}

	if (ret < 0) {
		printk("newchrled chrdev region err!!\r\n");
		goto fail_devid;
	}
	printk("newchrled major=%d, minor=%d\r\n", new_chr_led.major, new_chr_led.minor);

	/* 注册设备 */
	new_chr_led.cdev.owner = THIS_MODULE;
	cdev_init(&new_chr_led.cdev, &new_chr_led_fops);
	
	ret = cdev_add(&new_chr_led.cdev, new_chr_led.devid, 1);
	if (ret < 0) {
        goto fail_cdev;
	}

	/* 自动创建设备节点 */
	new_chr_led.class = class_create(THIS_MODULE, NEW_CHR_LED_NAME);
	if (IS_ERR(new_chr_led.class)) {
		ret = PTR_ERR(new_chr_led.class);
		goto fail_class;
	}

	new_chr_led.device = device_create(new_chr_led.class, NULL, new_chr_led.devid, NULL, NEW_CHR_LED_NAME);
	if (IS_ERR(new_chr_led.device)) {
		ret = PTR_ERR(new_chr_led.device);
		goto fail_device;
	}

	return 0;

fail_device:
	class_destroy(new_chr_led.class);
fail_class:
    cdev_del(&new_chr_led.cdev);
fail_cdev:
	unregister_chrdev_region(new_chr_led.devid, 1);

fail_devid:
	return ret;
}

/* 出口 */
static void __exit new_chr_led_exit(void)
{
	u32 val = 0;

	printk("new chr led exit!\r\n");

    //设置IO值，清0
	val = readl(GPIO1_DR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	/* 取消地址映射 */
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	/* 1.删除字符设备号 */
	cdev_del(&new_chr_led.cdev);
	
	/* 销毁设备 */
	device_destroy(new_chr_led.class, new_chr_led.devid);
	
	/* 2.卸载字符设备 */
	unregister_chrdev_region(new_chr_led.devid, 1);

	/* 4.销毁类 */
	class_destroy(new_chr_led.class);
}

/* 驱动挂载与卸载 */
module_init(new_chr_led_init);
module_exit(new_chr_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");