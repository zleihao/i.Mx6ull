KERNELDIR := /home/hao/linux/imx6ull/linux/temp/linux-imx-5.4.47

CURRENT_PATH := $(shell pwd)
obj-m := usbmouse_as_key.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean