#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/fcntl.h>


#define MOL_USB_MAX_PROTOCOLS 1

#define MOL_USB_DeviceClass     0xFF
#define MOL_USB_DeviceSubClass  0x00
#define MOL_USB_DeviceProtocol  0x00

//接口
#define MOL_USB_InterfaceClass     0xFF
#define MOL_USB_InterfaceSubClass  0x01
#define MOL_USB_InterfaceProtocol  0x02

static struct usb_driver mol_usb_device;


static struct usb_device_id mol_usb_id_table [] = {
    { USB_DEVICE_INFO(MOL_USB_DeviceClass, MOL_USB_DeviceSubClass, MOL_USB_DeviceProtocol) }, 
 	{ USB_INTERFACE_INFO(MOL_USB_InterfaceClass, MOL_USB_InterfaceSubClass, MOL_USB_InterfaceProtocol) }, 
	{ }	/* Terminating entry */
};

struct mol_usb {
    const struct usb_device_id *id;
    struct usb_device *dev;
    struct urb *urb;
	
	int			ifnum;			/* Interface number */
	struct usb_interface	*intf;			/* The interface */
	/* Alternate-setting numbers and endpoints for each protocol
	 * (7/1/{index=1,2,3}) that the device supports: */
	struct {
		int				alt_setting;
		struct usb_endpoint_descriptor	*epint_read;
		struct usb_endpoint_descriptor	*epwrite;
		struct usb_endpoint_descriptor	*epread;
	}	device_info[MOL_USB_MAX_PROTOCOLS];
	int			current_protocol;
};

int mol_usb_device_open(struct inode *inode, struct file *file)
{	
	int minor = iminor(inode);
	struct mol_usb *mol_usb;
	struct usb_interface *intf;

	if (minor < 0)
		return -ENODEV;

	intf = usb_find_interface(&mol_usb_device, minor);
	if (!intf) {
        printk(KERN_ERR "mol_usb_device_open: Unable to find interface for minor %d\n", minor);
        return -ENODEV;
    }
	mol_usb = usb_get_intfdata(intf);
	if (!mol_usb) {
        printk(KERN_ERR "mol_usb_device_open: No device data found for interface\n");
        return -ENODEV;
    }

	file->private_data = mol_usb;

	return 0;
}

ssize_t mol_usb_device_read(struct file *file, char __user *buf, size_t cnt, loff_t *size)
{
	return 0;
}

long mol_usb_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}


static void mol_bulk_write(struct urb *urb)
{
	usb_free_urb(urb);
}

ssize_t mol_usb_device_write(struct file *file, const char __user *buf, size_t cnt, loff_t *size)
{	
	struct mol_usb *mol_usb = file->private_data;
	struct urb *urb;
	int rv;

	char *writebuf;

	if ((writebuf = kmalloc(cnt, GFP_KERNEL)) == NULL)
		return -ENOMEM;
	if ((urb = usb_alloc_urb(0, GFP_KERNEL)) == NULL) {
		kfree(writebuf);
		return -ENOMEM;
	}

	if (copy_from_user(writebuf,
				   buf, cnt)) {
		printk("copy_from_user fail...\r\n");
		return -ENOMEM;
	}

	usb_fill_bulk_urb(urb, mol_usb->dev,
		usb_sndbulkpipe(mol_usb->dev,
		 mol_usb->device_info[0].epwrite->bEndpointAddress),
		writebuf, cnt, mol_bulk_write, mol_usb);
	//表明在 URB 完成后，内存将被释放			   
	urb->transfer_flags |= URB_FREE_BUFFER;


	rv = usb_submit_urb(urb, GFP_KERNEL);
	if (rv) {
		printk("usb_submit_urb failed: %d\n", rv);
		usb_free_urb(urb);
		return rv == -ENOMEM ? -ENOMEM : -EIO;
	}
				   
	return cnt;
}

int mol_usb_device_close(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}


struct file_operations mol_usb_fop = {
	.owner          = THIS_MODULE,
	.open           = mol_usb_device_open,
	.unlocked_ioctl = mol_usb_device_ioctl,
	.read           = mol_usb_device_read,
	.write          = mol_usb_device_write,
	.release        = mol_usb_device_close,
	
};


struct miscdevice misc_mol_usb = {
	.name = "mol_usb_device",
	.minor = 144,
	.fops = &mol_usb_fop,
};

static int mol_usb_select_alts(struct mol_usb *mol_usb)
{
	struct usb_interface *if_alt;
	struct usb_host_interface *ifd;
	struct usb_endpoint_descriptor *epd, *epwrite, *epread;
	int i, e;

	if_alt = mol_usb->intf;

	for (i = 0; i < if_alt->num_altsetting; i++) {
		ifd = &if_alt->altsetting[i];

		/* Look for bulk OUT and IN endpoints. */
		epwrite = epread = NULL;
		for (e = 0; e < ifd->desc.bNumEndpoints; e++) {
			epd = &ifd->endpoint[e].desc;

			if (usb_endpoint_is_bulk_out(epd))
				if (!epwrite) {
					epwrite = epd;
					printk("bulk out...\r\n");
				}
			if (usb_endpoint_is_bulk_in(epd))
				if (!epread) {
					epread = epd;
					printk("bulk in...\r\n");
				}
		}
		
 		mol_usb->device_info[i].alt_setting = ifd->desc.bAlternateSetting;
		mol_usb->device_info[i].epwrite = epwrite;
		mol_usb->device_info[i].epread = epread;
		//设置备用接口
		usb_set_interface(mol_usb->dev, mol_usb->ifnum, mol_usb->device_info[i].alt_setting);
	}

	return 0;
}

static char *mol_usb_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev));
}

static struct usb_class_driver mol_usb_class = {
	.name =		"mol_usb%d",
	.devnode =	mol_usb_devnode,
	.fops =		&mol_usb_fop,
	.minor_base =	0,
};


static int mol_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int err = 0;
	struct mol_usb *mol_usb;
	struct usb_device *udev = interface_to_usbdev(intf);

	printk("%s %s %d\r\n", __FILE__, __func__, __LINE__);
	
	mol_usb = kmalloc(sizeof(struct mol_usb), GFP_KERNEL);
	//记录 usb 信息
	mol_usb->dev   = udev;
	mol_usb->ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	mol_usb->intf  = intf;

	mol_usb_select_alts(mol_usb);

	//存放数据
	usb_set_intfdata(intf, mol_usb);

	err = usb_register_dev(intf, &mol_usb_class);
	if (err) {
		dev_err(&intf->dev,
			"usblp: Not able to get a minor (base %u, slice default): %d\n",
			0, err);
	}
	
	return 0;
}

static void mol_usb_disconnect(struct usb_interface *intf)
{
	struct mol_usb *mol_usb = usb_get_intfdata(intf);

	printk("%s %s %d\r\n", __FILE__, __func__, __LINE__);

	usb_set_intfdata(intf, NULL);

	kfree(mol_usb);

	usb_deregister_dev(intf, &mol_usb_class);

	//misc_deregister(&misc_mol_usb);
}

static struct usb_driver mol_usb_device = {
	.name       = "master of lighting usb device",
    .probe      = mol_usb_probe,
    .disconnect = mol_usb_disconnect,
    .id_table   = mol_usb_id_table,
};
	
static int __init mol_usb_device_init(void)
{
	return usb_register(&mol_usb_device);
}

static void __exit mol_usb_device_exit(void)
{
	usb_deregister(&mol_usb_device);
}

module_init(mol_usb_device_init);
module_exit(mol_usb_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Master of Lighting");
