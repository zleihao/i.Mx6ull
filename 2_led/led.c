#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

//主设备号
#define LED_MAJOR   200
//设备名字
#define LED_NAME    "led"

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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
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
static int led_open(struct inode *inode, struct file *filp)
{
	return 0;
}

//close led dev
static int led_close(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 字符设备操作集 */
static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.write = led_write,
	.open = led_open,
	.release = led_close
};

/* 入口 */
static int __init led_init(void)
{
	int ret = 0;
	u32 val = 0;

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

	/* 注册字符设备 */
	ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
	if (ret < 0) {
		printk("register led dev fail!\r\n");
		return -EIO;
	}


	return 0;
}

/* 出口 */
static __exit void led_exit(void)
{
	u32 val = 0;
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

	/* 注销led字符设备 */
	unregister_chrdev(LED_MAJOR, LED_NAME);
	printk("led_exit\r\n");
}

/* 注册函数与注销函数 */
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");