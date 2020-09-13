MODULE_NAME = gpio_driver
# EXTRA_CFLAGS += -Werror


obj-m   := $(MODULE_NAME).o


KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)


module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

help:
	$(MAKE) -C $(KDIR) M=$(PWD) help

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

