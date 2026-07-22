MODULE_MAIN_OBJ := module.o
COMMON_MODULES := utils gpio atecc
MODULE_EXTRA_OBJS := rtc-pcf2131/module.o
UDEV_RULES := 99-stratopiplus.rules

SOURCE_DIR := $(if $(src),$(src),$(CURDIR))
include $(SOURCE_DIR)/commons/scripts/kmod-common.mk
