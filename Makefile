obj-m := synccom.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
IGNORE :=
synccom-objs := src/synccommain.o src/port.o src/card.o src/isr.o src/utils.o \
             src/frame.o src/sysfs.o src/descriptor.o src/debug.o \
             src/flist.o

ifeq ($(DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG
endif

ifeq ($(RELEASE_PREVIEW),1)
	EXTRA_CFLAGS += -DRELEASE_PREVIEW
endif

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install:
	cp synccom.rules /etc/udev/rules.d/
	cp synccom.ko /lib/modules/`uname -r`/kernel/drivers/char/
	echo 'synccom' >> /etc/modules
	cp lib/raw/synccom.h /usr/local/include/
	depmod

uninstall:
	rm /etc/udev/rules.d/synccom.rules
	rm /lib/modules/`uname -r`/kernel/drivers/char/synccom.ko
	sed --in-place '/synccom/d' /etc/modules
	rm /usr/local/include/synccom.h
	depmod

clean:
	@find . $(IGNORE) \
	\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \
		-o -name '*.markers' -o -name '*.symvers' -o -name '*.order' \
		-o -name '*.tmp_versions' \) \
		-type f -print | xargs rm -f
	rm -rf .tmp_versions

help:
	@echo
	@echo 'Build targets:'
	@echo '  make - Build driver module'
	@echo '  make clean - Remove most generated files'
	@echo '  make install - Installs SYNCCOM driver and C header file'
	@echo '  make uninstall - Clean up installation'
	@echo
