#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

int main(int argc, char **argv)
{
    int err;
    int num_devices;
    int endpoint = 0;
    int found = 0;
    int interface_num;
    int actual_length;
    int count = 0;
    unsigned char buff[8];
    struct libusb_device_handle *dev_handle = NULL;
    libusb_device *dev, **devs;
    struct libusb_config_descriptor *config_desc;
    
    /* libusb init */
    err = libusb_init(NULL);
    if (err < 0) {
		fprintf(stderr, "failed to initialise libusb");
		exit(1);
	}

    /* get device list */
    num_devices = libusb_get_device_list(NULL, &devs);
    if (num_devices < 0) {
        fprintf(stderr, "libusb_get_device_list() failed\n");
        libusb_exit(NULL);
        exit(1);
    }

	fprintf(stdout, "libusb-get-device-list ok\n");

    for (int i = 0; i < num_devices; i++) {
        dev = devs[i];
        err = libusb_get_config_descriptor(dev, 0, &config_desc);
        if (err) {
            fprintf(stderr, "could not get configuration descriptor\n");
            continue;
        }
		fprintf(stdout, "libusb-get-config-descriptor ok\n");

        for (int interface = 0; interface < config_desc->bNumInterfaces; interface++) {
            const struct libusb_interface_descriptor *intf_desc = &config_desc->interface[interface].altsetting[0];
            interface_num = intf_desc->bInterfaceNumber;

            if (intf_desc->bInterfaceClass != 3 || intf_desc->bInterfaceProtocol != 2) {
                continue;
            } else {
                /* 找到了USB鼠标 */
				fprintf(stdout, "find usb mouse ok\n");
                for (int ep = 0; ep < intf_desc->bNumEndpoints; ep++) {
                    // if ((intf_desc->endpoint[ep].bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_INTERRUPT ||
					// 	(intf_desc->endpoint[ep].bEndpointAddress & 0x80) != LIBUSB_ENDPOINT_IN) {
                    //     /* 找到了输入的中断端点 */
                    //     fprintf(stdout, "find in int endpoint\n");
                    //     endpoint = intf_desc->endpoint[ep].bEndpointAddress;
                    //     found = 1;
                    //     printf("found = %d\n", found);
                    //     break;
                    // }
                     if ((intf_desc->endpoint[ep].bmAttributes & 3) == LIBUSB_TRANSFER_TYPE_INTERRUPT ||
                            (intf_desc->endpoint[ep].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
                        /* 找到了输入的中断端点 */
                        fprintf(stdout, "find in int endpoint\n");
                        endpoint = intf_desc->endpoint[ep].bEndpointAddress;
                        found = 1;
                        break;
                    }
                }
            }
            if (found) {
                break;
            }
        }
        libusb_free_config_descriptor(config_desc);
        if (found) {
            break;
        }
    }
    
    if (!found)
    {
        fprintf(stdout, "usb mouse not find\n");
        /* free device list */
        libusb_free_device_list(devs, 1);
        libusb_exit(NULL);
        exit(1);
    }

    if (found) {
        err = libusb_open(dev, &dev_handle);
        if (err) {
            fprintf(stderr, "could not open usb mouse device\n");
            exit(1);
        }
        fprintf(stdout, "libusb_open ok\n");
    }

    libusb_free_device_list(devs, 1);

    libusb_set_auto_detach_kernel_driver(dev_handle, 1);
    err = libusb_claim_interface(dev_handle, interface_num);
    if (err) {
        fprintf(stderr, "Fail to libusb_claim_interface\n");
        exit(1);
    }
    fprintf(stdout, "libusb_claim_interface ok\n");

    while (1) {
        err = libusb_interrupt_transfer(dev_handle, endpoint, buff, 8, &actual_length, 0);
        if (err) {
            if (actual_length >= 4) {
                printf("%04d datas: %02x %02x %02x %02x\n", count++, buff[0], buff[1], buff[2], buff[3]);
            }
        } else {
            fprintf(stderr, "Fail to libusb_interrupt_transfer\n");
            exit(1);
        }
    }

    libusb_release_interface(dev_handle, interface_num);
    libusb_close(dev_handle);
    libusb_exit(NULL);   
}
