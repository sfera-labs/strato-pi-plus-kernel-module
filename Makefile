MODULE_MAIN_OBJ := module.o
COMMON_MODULES := utils gpio atecc pcf21xx
UDEV_RULES := 99-stratopiplus.rules
UDEV_GROUP := stratopiplus

SOURCE_DIR := $(if $(src),$(src),$(CURDIR))
include $(SOURCE_DIR)/commons/scripts/kmod-common.mk
