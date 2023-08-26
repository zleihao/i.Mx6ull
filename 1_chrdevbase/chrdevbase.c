#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR  200  //主设备号
#define CHRDEVBASE_NAME   "chrdevbase"

static char read_buf[100];
static char write_buf[100];

static char kernel_data[] = "This kernel file!";
 
static int chrdevbase_open(struct inode *inode, struct file *filp)
{
	// printk("chrdevbase open\r\n");
	return 0;
}

static int chrdevbase_close(struct inode *inode, struct file *filp)
{
	// printk("chrdevbase close\r\n");
	return 0;
}

static ssize_t chrdevbase_read(struct file *filp, __user char *buf, size_t count, loff_t *ppos) 
{
	// printk("chrdevbase read\r\n");
	int ret = 0;
	memcpy(read_buf, kernel_data, sizeof(kernel_data));

	ret = copy_to_user(buf, read_buf, count);
	if (ret == 0) {

	} else {

	}

	return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	// printk("chrdevbase write\r\n");
	int ret = 0;
	ret = copy_from_user(write_buf, buf, count);
	if (ret == 0) {
		printk("Kernel recevdata: %s\r\n", write_buf);
	} else {

	}
	return 0;
}

static struct file_operations chrdevbase_fops = {
	.owner = THIS_MODULE,
	.open = chrdevbase_open,
	.release = chrdevbase_close,
	.read = chrdevbase_read,
	.write = chrdevbase_write,
};

static int __init chrdevbase_init(void)
{
	int ret = 0;
	printk("chrdevbase_init\r\n");

	/* 注册字符设备 */
	ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
	if (ret < 0) {
		printk("chrdevbase init fail!\r\n");
	}

	return 0;
}

static void __exit chrdevbase_exit(void)
{
	printk("chrdevbase_exit\r\n");

	unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
}

/* 模块入口与出口 */
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");