#
# Copyright 2009 Nexell Co.
#
# Makefile for the S5P6818

obj-y					:= board.o device.o gec6818-lcds.o
#obj-y					:= board.o device.o gec6818-lcds.o
obj-$(CONFIG_SUSPEND)			+= pm.o
obj-$(CONFIG_ION_NXP)			+= dev-ion.o
obj-$(CONFIG_USB_CONNECT_NXP_DRV)	+= usb-connector.o

