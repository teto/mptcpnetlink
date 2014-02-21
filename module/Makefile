# TARGET = mptcp_nl


ifneq ($(KERNELRELEASE),)

obj-m += mptcp_nl.o 
mptcp_nl-y := mptcp_nl_simple.o mptcp_netlink.o
#mptcp_mesh.o
else

# KDIR ?= /home/teto/mptcp88/x86
KDIR ?= /lib/modules/`uname -r`/build
# normal makefile
all: default

clean: 
	#$(MAKE) -C $(KDIR) M=$(PWD) clean

default: 
	$(MAKE) -C $(KDIR) M=$(PWD)

.PHONY: default

endif
