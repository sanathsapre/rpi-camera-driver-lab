obj-m += sanath-v4l2-lab10.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install
