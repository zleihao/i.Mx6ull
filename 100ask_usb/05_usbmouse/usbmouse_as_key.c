#include "linux/device.h"
#include "linux/kdev_t.h"
#include "linux/printk.h"
#include "linux/usb.h"
#include "uapi/linux/input.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/input.h>

struct usb_mouse_as_key_desc {
	struct usb_interface *intf;
	const struct usb_device_id *id;
	struct input_dev *input;
	struct usb_device *usbdev;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_host_interface *interface;
	struct urb *urb;
	int pipe, maxp;

	void *data;
	dma_addr_t data_dma;
};

static const struct usb_device_id usb_mouse_as_key_id_table[] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, usb_mouse_as_key_id_table);

void usb_mouse_as_key_irq(struct urb *urb)
{
	//上报input event
	struct input_dev *dev = urb->context;
    struct usb_mouse_as_key_desc *desc = input_get_drvdata(dev);
    signed char *data = desc->data;
    int status;

    switch (urb->status) {
    case 0:                 /* success */
            break;
    case -ECONNRESET:       /* unlink */
    case -ENOENT:
    case -ESHUTDOWN:
            return;
    /* -EPIPE:  should clear the halt */
    default:                /* error */
            goto resubmit;
    }
    input_report_key(dev, KEY_L,     data[1] & 0x01);
    input_report_key(dev, KEY_S,     data[1] & 0x02);
    input_report_key(dev, KEY_ENTER, data[1] & 0x04);

    input_sync(dev);
resubmit:
    status = usb_submit_urb(urb, GFP_ATOMIC);
}


int usb_mouse_as_key_open(struct input_dev *dev)
{
	//得到usb数据
	struct usb_mouse_as_key_desc *desc = (struct usb_mouse_as_key_desc *)input_get_drvdata(dev);
	
	//分配/填充/设置 urb
	struct urb *urb;
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		printk("usb_alloc_urb fail\n");
		return -1;
	}
	desc->urb = urb;

	//再次将desc放置到input_dev中,增加urb
	input_set_drvdata(dev, desc);

	//填充
	usb_fill_int_urb(desc->urb, desc->usbdev, desc->pipe, desc->data, 
		             (desc->maxp > 8 ? 8 : desc->maxp), 
		             usb_mouse_as_key_irq, dev, desc->endpoint->bInterval);

	urb->transfer_dma = desc->data_dma;
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	
	//提交urb
	if (usb_submit_urb(desc->urb, GFP_KERNEL))
		return -EIO;
	
	return 0;
}

void usb_mouse_as_key_close(struct input_dev *dev)
{
	struct usb_mouse_as_key_desc *desc = (struct usb_mouse_as_key_desc *)input_get_drvdata(dev);

	//取消/释放 urb
	usb_kill_urb(desc->urb);
	usb_free_urb(desc->urb);
}

static int usb_mouse_as_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct usb_host_interface *interface;
	struct input_dev *input_dev;
	struct usb_mouse_as_key_desc *desc;
	int error;
	int pipe, maxp;

	printk("%s %s %d\n", __FILE__, __func__, __LINE__);

	/**
	 * 1，分配/设置/注册 input_dev
	 * 2, 设置 input_dev 触发哪类事件
	 * 3，在 input_dev 的 open 函数里面，分配/填充/提交 URB
	 * 4, 在 URB 的回调函数中分析/上报 input even.
	 */
	interface = intf->cur_altsetting;
	endpoint = &interface->endpoint[0].desc;

	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	desc = kmalloc(sizeof(struct usb_mouse_as_key_desc), GFP_KERNEL);
	if (!desc) {
		printk("struct usb_mouse_as_key_desc alloc fail\n");
		return 0;
	}

	input_dev = devm_input_allocate_device(&intf->dev);

	//记录 intf、id
	desc->intf = intf;
	desc->id = id;
	desc->input = input_dev;
	desc->pipe = pipe;
	desc->maxp = maxp;
	desc->usbdev = dev;
	desc->endpoint = endpoint;
	desc->interface = interface;
	desc->data = usb_alloc_coherent(desc->usbdev, 8, GFP_ATOMIC, &desc->data_dma);
	
	input_set_drvdata(input_dev, desc);

	//产生什么类型事件
	__set_bit(EV_KEY, input_dev->evbit);

	//设置产生哪些事件
	__set_bit(KEY_L, input_dev->keybit);
	__set_bit(KEY_S, input_dev->keybit);	
	__set_bit(KEY_ENTER, input_dev->keybit);

	input_dev->open  = usb_mouse_as_key_open;
	input_dev->close = usb_mouse_as_key_close;

	error = input_register_device(input_dev);
	if (error < 0) {
		printk("input_register_device fail\n");
		//free(desc);
		return -1;
	}

	usb_set_intfdata(intf, input_dev);
	
	return 0;
}


static void usb_mouse_as_key_disconnect(struct usb_interface *intf)
{
	struct input_dev *input_dev = (struct input_dev *)usb_get_intfdata(intf);	
	struct usb_mouse_as_key_desc *desc = (struct usb_mouse_as_key_desc *)input_get_drvdata(input_dev);

	printk("%s %s %d\n", __FILE__, __func__, __LINE__);

	usb_set_intfdata(intf, NULL);
	if (desc) {
		input_unregister_device(input_dev);
		usb_free_coherent(interface_to_usbdev(intf), 8, desc->data, desc->data_dma);
		kfree(desc);
	}
}


static struct usb_driver usb_mouse_as_key_driver = {
	.name		= "usbmouse",
	.probe		= usb_mouse_as_key_probe,
	.disconnect	= usb_mouse_as_key_disconnect,
	.id_table	= usb_mouse_as_key_id_table,
};

//usb出口/入口
module_usb_driver(usb_mouse_as_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lighting_master");




