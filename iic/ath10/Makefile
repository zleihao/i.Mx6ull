KERNELDIR := /home/hao/linux/imx6ull/linux/temp/linux-fslc-5.4-2.1.x-imx 

CURRENT_PATH := $(shell pwd)
obj-m := ath10.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
