KERNELDIR := /home/hao/linux/imx6ull/linux/temp/linux-alientek-emmc

CURRENT_PATH := $(shell pwd)
obj-m := mol_usb_serial.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean