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
    struct usb_device *dev;
    struct urb *urb;
    int pipe, maxp;
    int bInterval;
    void *data_buf;
    dma_addr_t data_dma;
};

static struct usb_device_id usb_mouse_as_key_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};

static void usb_mouse_as_key_irq(struct urb *urb)
{
	struct input_dev *dev = urb->context;
    struct usb_mouse_as_key_desc *desc = input_get_drvdata(dev);
	signed char *data = desc->data_buf;
	int status;

	switch (urb->status) {
	case 0:			/* success */
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}

	input_report_key(dev, KEY_L,     data[1] & 0x01);
	input_report_key(dev, KEY_S,     data[1] & 0x02);
	input_report_key(dev, KEY_ENTER, data[1] & 0x04);

	input_sync(dev);
resubmit:
	status = usb_submit_urb (urb, GFP_ATOMIC);
}

int usb_mouse_as_key_open(struct input_dev *dev)
{
    struct urb *urb;
    struct usb_mouse_as_key_desc *desc = input_get_drvdata(dev);

    printk("%s %d %s\n", __FILE__, __LINE__, __func__);

    /* 分配/填充/提交 URB */
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb) {
        printk("urb alloc fail\n");
        return -1;
    }

    desc->urb = urb;

    usb_fill_int_urb(urb, 
                    desc->dev, 
                    desc->pipe, 
                    desc->data_buf, (desc->maxp > 8 ? 8 : desc->maxp),
                    usb_mouse_as_key_irq,
                    dev, 
                    desc->bInterval);

    urb->transfer_dma = desc->data_dma; // usb_alloc_coherent的输出参数
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    /* 提交urb */
    usb_submit_urb (urb, GFP_ATOMIC);

    return 0;
}

void usb_mouse_as_key_close(struct input_dev *dev)
{
    struct usb_mouse_as_key_desc *desc = input_get_drvdata(dev);
    
    /* 取消/释放 URB */
    usb_kill_urb(desc->urb);
    usb_free_urb(desc->urb);
}

/* 1. 记录设备信息：intf */
/**
 * 2. 分配/设置/注册input_dev
 * 2.1 能产生哪类事件
 * 2.2 能产生这类事件里哪些事件：L/S/ENTER
 * 2.3 设置函数，比如open/close
 * 2.4 在open函数里：分配/填充/提交 URB
 * 2.5 URB的回调函数：解析数据，上报input_event
 */
static int usb_mouse_as_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_mouse_as_key_desc *desc;
    struct input_dev *input_dev;
    struct usb_endpoint_descriptor *endpoint;
    struct usb_host_interface *interface;
    int error;
    int pipe, maxp;

    struct usb_device *dev = interface_to_usbdev(intf);
    
    interface = intf->cur_altsetting;

	if (interface->desc.bNumEndpoints != 1)
		return -ENODEV;
    endpoint = &interface->endpoint[0].desc;
	if (!usb_endpoint_is_int_in(endpoint))
		return -ENODEV;

    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

    /* 1. 记录设备信息：intf */
    desc = kmalloc(sizeof(struct usb_mouse_as_key_desc), GFP_KERNEL);
    desc->intf = intf;
    desc->id = id;
    desc->dev = dev;
    desc->pipe = pipe;
    desc->maxp = maxp;
    desc->bInterval = endpoint->bInterval;
    desc->data_buf = usb_alloc_coherent(dev, maxp, GFP_ATOMIC, &desc->data_dma);
   
    input_dev = devm_input_allocate_device(&intf->dev);
    
    input_set_drvdata(input_dev, desc);

    /* 分配/设置/注册input_dev */
    /* 1.能产生哪类事件 */
    __set_bit(EV_KEY, input_dev->evbit);

    /* 2.能产生这类事件里哪些事件：L/S/ENTER */
    __set_bit(KEY_L, input_dev->keybit);
    __set_bit(KEY_S, input_dev->keybit);
    __set_bit(KEY_ENTER, input_dev->keybit);

    /* 设置open/close */
    input_dev->open = usb_mouse_as_key_open;
    input_dev->close = usb_mouse_as_key_close;

    /* 注册input_dev */
    error = input_register_device(input_dev);
    if (error) {
        printk("input_dev register fail\n");
        return -1;
    }
    
    usb_set_intfdata(intf, input_dev);
    return 0;
}

static void usb_mouse_as_key_disconnect(struct usb_interface *intf)
{
    struct input_dev *input_dev = usb_get_intfdata(intf);
    struct usb_mouse_as_key_desc *desc = input_get_drvdata(input_dev);

    usb_set_intfdata(intf, NULL);
    
    usb_free_coherent(desc->dev, desc->maxp, desc->data_buf, desc->data_dma);
    kfree(desc);
    input_unregister_device(input_dev);
}

static struct usb_driver usb_mouse_driver = {
	.name		= "usbmouse_as_key",
	.probe		= usb_mouse_as_key_probe,
	.disconnect	= usb_mouse_as_key_disconnect,
	.id_table	= usb_mouse_as_key_id_table,
};

static int __init usbmouse_ask_key_init(void)
{
    return usb_register(&usb_mouse_driver);
}

static void __exit usbmouse_ask_key_exit(void)
{
    usb_deregister(&usb_mouse_driver);
}

module_init(usbmouse_ask_key_init);
module_exit(usbmouse_ask_key_exit);
MODULE_AUTHOR("lighting master");
MODULE_LICENSE("GPL");
