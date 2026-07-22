/*
 * stratopiplus
 *
 *     Copyright (C) 2026 Sfera Labs S.r.l.
 *
 *     For information, visit https://www.sferalabs.cc
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * LICENSE.txt file for more details.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/version.h>

#include "commons/atecc/atecc.h"
#include "commons/gpio/gpio.h"
#include "commons/utils/utils.h"
#include "rp2_i2c.h"
#include "rtc-pcf2131/pcf21xx.h"

#define pr_fmt(fmt) "stratopiplus: " fmt

#define STRATOPIPLUS_MAX_MINORS 256

struct DeviceAttrRegSpecs {
  uint8_t reg;
  uint8_t len;
  uint32_t mask;
  uint8_t shift;
  bool sign;
  uint8_t base;
};

struct DeviceAttrBean {
  struct device_attribute devAttr;
  struct DeviceAttrRegSpecs regSpecs;
  struct DeviceAttrRegSpecs regSpecsStore;
  struct GpioBean* gpio;
  const char* vals;
};

struct DeviceBean {
  char* name;
  struct DeviceAttrBean* devAttrBeans;
  struct device* device;
  dev_t devt;
  int minor;
};

struct DynamicDevice {
  struct DeviceBean* db;
  struct device* device;
  dev_t devt;
  int minor;
  struct DynamicDevice* next;
  void* data;
};

static ssize_t _devAttrI2c_store(struct device* dev,
                                 struct device_attribute* attr, const char* buf,
                                 size_t count);

static ssize_t _devAttrI2c_show(struct device* dev,
                                struct device_attribute* attr, char* buf);

static ssize_t _devAttrI2c_show_clear(struct device* dev,
                                      struct device_attribute* attr, char* buf);

static ssize_t _devAttrFwVersion_show(struct device* dev,
                                      struct device_attribute* attr, char* buf);

static ssize_t _devAttrConfig_store(struct device* dev,
                                    struct device_attribute* attr,
                                    const char* buf, size_t count);

static ssize_t _devAttrI2cRead_show(struct device* dev,
                                    struct device_attribute* attr, char* buf);

static ssize_t _devAttrI2cRead_store(struct device* dev,
                                     struct device_attribute* attr,
                                     const char* buf, size_t count);

static ssize_t _devAttrI2cWrite_store(struct device* dev,
                                      struct device_attribute* attr,
                                      const char* buf, size_t count);

static ssize_t _devAttrBlink_store(struct device* dev,
                                   struct device_attribute* attr,
                                   const char* buf, size_t count);

static struct DeviceAttrBean _devAttrBeansSystem[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "fw_version",
                        .mode = 0440,
                    },
                .show = _devAttrFwVersion_show,
                .store = NULL,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "config",
                        .mode = 0220,
                    },
                .show = NULL,
                .store = _devAttrConfig_store,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_errs",
                        .mode = 0440,
                    },
                .show = _devAttrI2c_show_clear,
                .store = NULL,
            },
        .regSpecs =
            {
                .reg = I2C_REG_SYSMON_SYS_ERRS,
                .len = 2,
                .mask = 0xffff,
                .base = 2,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "_i2c_read",
                        .mode = 0660,
                    },
                .show = _devAttrI2cRead_show,
                .store = _devAttrI2cRead_store,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "_i2c_write",
                        .mode = 0220,
                    },
                .show = NULL,
                .store = _devAttrI2cWrite_store,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansPower[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "down_enabled",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_POWER_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "pcie_switch_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_POWER_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 3,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "down_delay_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_POWER_DOWN_DELAY,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "off_time_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_POWER_OFF_TIME,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "up_delay_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_POWER_UP_DELAY,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansWatchdog[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "enabled",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "enabled_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 1,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "heartbeat",
                        .mode = 0220,
                    },
                .show = NULL,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 2,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "expired",
                        .mode = 0440,
                    },
                .show = _devAttrI2c_show,
                .store = NULL,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_MAIN,
                .len = 2,
                .mask = 0b1,
                .shift = 2,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "timeout",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_TIMEOUT,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "timeout_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_TIMEOUT_CFG,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "down_delay_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_DOWN_DELAY,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "pcie_switch_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_WDT_PCIE_SWITCH_CNT,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansLed[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "on",
                        .mode = 0220,
                    },
                .show = NULL,
                .store = _devAttrBlink_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_T_ON,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_pwr_up_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_pwr_down_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 1,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_pwr_off_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 2,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_wd_en_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 3,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "sys_wd_exp_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 4,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "user_priority_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_LED_CFG,
                .len = 2,
                .mask = 0b1,
                .shift = 15,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansPowerIn[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "mon_v",
                        .mode = 0440,
                    },
                .show = _devAttrI2c_show,
                .store = NULL,
            },
        .regSpecs =
            {
                .reg = I2C_REG_SYSMON_VIN_V,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "mon_i",
                        .mode = 0440,
                    },
                .show = _devAttrI2c_show,
                .store = NULL,
            },
        .regSpecs =
            {
                .reg = I2C_REG_SYSMON_VIN_I,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansAtecc[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "serial_num",
                        .mode = 0440,
                    },
                .show = devAttrAteccSerial_show,
                .store = NULL,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansFan[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "temp",
                        .mode = 0440,
                    },
                .show = _devAttrI2c_show,
                .store = NULL,
            },
        .regSpecs =
            {
                .reg = I2C_REG_SYSMON_RP2_TEMP,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = true,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "temp_on",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_FAN_TEMP_ON,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = true,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "temp_off",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_FAN_TEMP_OFF,
                .len = 2,
                .mask = 0,
                .shift = 0,
                .sign = true,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansRs485[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "mode_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = 0,
                .len = 2,
                .mask = 0b11,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "termination_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = 0,
                .len = 2,
                .mask = 0b1,
                .shift = 8,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "inversion_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = 0,
                .len = 2,
                .mask = 0b1,
                .shift = 9,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "echo_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = 0,
                .len = 2,
                .mask = 0b1,
                .shift = 10,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansPcie[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "enabled",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_PCIE,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "enabled_config",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_PCIE,
                .len = 2,
                .mask = 0b1,
                .shift = 1,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansRelay[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "status",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_RELAY,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {},
};

static struct DeviceAttrBean _devAttrBeansCan[] = {
    {
        .devAttr =
            {
                .attr =
                    {
                        .name = "termination",
                        .mode = 0660,
                    },
                .show = _devAttrI2c_show,
                .store = _devAttrI2c_store,
            },
        .regSpecs =
            {
                .reg = I2C_REG_CAN_TERM,
                .len = 2,
                .mask = 0b1,
                .shift = 0,
                .sign = false,
            },
    },

    {},
};

static struct DeviceBean _devices[] = {
    {
        .name = "system",
        .devAttrBeans = _devAttrBeansSystem,
    },

    {
        .name = "power",
        .devAttrBeans = _devAttrBeansPower,
    },

    {
        .name = "watchdog",
        .devAttrBeans = _devAttrBeansWatchdog,
    },

    {
        .name = "led",
        .devAttrBeans = _devAttrBeansLed,
    },

    {
        .name = "power_in",
        .devAttrBeans = _devAttrBeansPowerIn,
    },

    {
        .name = "fan",
        .devAttrBeans = _devAttrBeansFan,
    },

    {
        .name = "sec_elem",
        .devAttrBeans = _devAttrBeansAtecc,
    },

    {
        .name = "pcie",
        .devAttrBeans = _devAttrBeansPcie,
    },

    {},
};

static uint8_t _rs485DevIdx[] = {0, 1, 2, 3};

static struct DeviceBean _rs485Dev = {
    .name = "rs485_s%d",
    .devAttrBeans = _devAttrBeansRs485,
};

static struct DeviceBean _canDev = {
    .name = "can",
    .devAttrBeans = _devAttrBeansCan,
};

static struct DeviceBean _relayDev = {
    .name = "relay",
    .devAttrBeans = _devAttrBeansRelay,
};

static uint8_t _fwVerMajor;
static uint8_t _fwVerMinor;
static struct class* _pDeviceClass;
static dev_t _devt_base;
static DEFINE_IDA(_minor_ida);

static DEFINE_MUTEX(_i2c_mtx);
static struct i2c_client* _rp2_i2c_client = NULL;
static bool _rp2_probed = false;
static uint8_t _hwCfg;
struct DynamicDevice* _dynDevs;

static int64_t _i2cReadVal;
static uint16_t _i2cReadSize;

struct GpioBean* gpioGetBean(struct device* dev, struct device_attribute* attr,
                             const char** vals) {
  struct DeviceAttrBean* dab;
  dab = container_of(attr, struct DeviceAttrBean, devAttr);
  if (dab == NULL) {
    return NULL;
  }
  *vals = dab->vals;
  return dab->gpio;
}

static bool _i2c_trylock(void) {
  uint8_t i;
  for (i = 0; i < 20; i++) {
    if (mutex_trylock(&_i2c_mtx)) {
      return true;
    }
    msleep(1);
  }
  return false;
}

static void _i2c_lock(void) { mutex_lock(&_i2c_mtx); }

static void _i2c_unlock(void) { mutex_unlock(&_i2c_mtx); }

static uint8_t _i2c_crc_process(uint8_t crc, uint8_t dByte) {
  uint8_t k;
  crc ^= dByte;
  for (k = 0; k < 8; k++) crc = crc & 0x80 ? (crc << 1) ^ 0x2f : crc << 1;
  return crc;
}

static void _i2c_add_crc(int reg, char* data, uint8_t len) {
  uint8_t i;
  uint8_t crc;

  crc = _i2c_crc_process(0xff, reg);
  for (i = 0; i < len; i++) {
    crc = _i2c_crc_process(crc, data[i]);
  }
  data[len] = crc;
}

static int64_t _i2c_read_no_lock(uint8_t reg, uint8_t len) {
  int64_t res;
  char buf[5];  // max 4 bytes data + 1 byte crc
  uint8_t i;
  uint8_t crc;

  if (_rp2_i2c_client == NULL) {
    return -ENODEV;
  }

  for (i = 0; i < 10; i++) {
    res = i2c_smbus_read_i2c_block_data(_rp2_i2c_client, reg, len + 1, buf);
    if (res == len + 1) {
      crc = buf[len];
      _i2c_add_crc(reg, buf, len);
      if (crc == buf[len]) {
        break;
      } else {
        res = -1;
      }
    }
  }

  if (res != len + 1) {
    return -EIO;
  }

  res = 0;
  for (i = 0; i < len; i++) {
    res |= (buf[i] & ((int64_t)0xff)) << (i * 8);
  }

  return res;
}

static int64_t _i2c_write_no_lock(uint8_t reg, uint8_t len, uint32_t val,
                                  uint32_t mask) {
  char buf[9];  // max 4 bytes data + 4 bytes mask + 1 byte crc
  uint8_t i;

  if (_rp2_i2c_client == NULL) {
    return -ENODEV;
  }

  for (i = 0; i < len; i++) {
    buf[i] = val >> (8 * i);
  }
  if (mask != 0) {
    for (i = 0; i < len; i++) {
      buf[i + len] = mask >> (8 * i);
    }
    len *= 2;
  }
  _i2c_add_crc(reg, buf, len);
  len++;

  for (i = 0; i < 10; i++) {
    if (!i2c_smbus_write_i2c_block_data(_rp2_i2c_client, reg, len, buf)) {
      return len;
    }
  }
  return -EIO;
}

static int64_t _i2c_read(uint8_t reg, uint8_t len) {
  int64_t res;

  if (len == 0) {
    return -EINVAL;
  }

  if (!_i2c_trylock()) {
    return -EBUSY;
  }

  res = _i2c_read_no_lock(reg, len);

  _i2c_unlock();

  return res;
}

static int64_t _i2c_write(uint8_t reg, uint8_t len, uint32_t val,
                          uint32_t mask) {
  int64_t res;

  if (!_i2c_trylock()) {
    return -EBUSY;
  }

  res = _i2c_write_no_lock(reg, len, val, mask);

  _i2c_unlock();

  return res;
}

static int64_t _i2c_read_segment(uint8_t reg, uint8_t len, uint32_t mask,
                                 uint8_t shift) {
  int64_t res;
  res = _i2c_read(reg, len);
  if (res < 0) {
    return res;
  }
  res >>= shift;
  if (mask != 0) {
    res &= mask;
  }
  return res;
}

static int64_t _i2c_write_segment(uint8_t reg, uint8_t len, uint32_t mask,
                                  uint8_t shift, uint32_t val, bool readBack) {
  int64_t res = 0;
  uint8_t i, j, b;
  bool ok;

  if (!_i2c_trylock()) {
    return -EBUSY;
  }

  val <<= shift;

  if (mask != 0) {
    mask <<= shift;
    val &= mask;
  }

  for (i = 0; i < 5; i++) {
    res = _i2c_write_no_lock(reg, len, val, mask);
    if (res >= 0) {
      if (readBack) {
        for (j = 0; j < 2; j++) {
          res = _i2c_read_no_lock(reg, len);
          if (res >= 0) {
            if (mask != 0) {
              res &= mask;
            }
            ok = true;
            for (b = 0; b < len; b++) {
              if (((val >> (b * 8)) & 0xff) != ((res >> (b * 8)) & 0xff)) {
                ok = false;
                break;
              }
            }
            if (ok) {
              res = len;
              break;
            }
            res = -EPERM;
          }
        }
        if (res >= 0) {
          break;
        }
      } else {
        break;
      }
    }
  }

  _i2c_unlock();

  return res;
}

static ssize_t _devAttrI2c_show(struct device* dev,
                                struct device_attribute* attr, char* buf) {
  struct DeviceAttrRegSpecs* specs;
  struct DeviceAttrBean* dab;
  void* data;
  uint8_t reg;
  int64_t res;

  data = dev_get_drvdata(dev);
  dab = container_of(attr, struct DeviceAttrBean, devAttr);
  if (dab == NULL) {
    return -EFAULT;
  }
  specs = &dab->regSpecs;
  if (specs->len == 0) {
    return -EFAULT;
  }

  reg = specs->reg;
  if (data != NULL) {
    reg += I2C_RS485_IDX_TO_REG_START(*((uint8_t*)data));
  }

  res = _i2c_read_segment(reg, specs->len, specs->mask, specs->shift);

  if (res < 0) {
    return res;
  }

  return valToStr(buf, res, dab->vals, specs->sign, specs->len, specs->base,
                  specs->mask);
}

static ssize_t _devAttrI2c_show_clear(struct device* dev,
                                      struct device_attribute* attr,
                                      char* buf) {
  ssize_t res = _devAttrI2c_show(dev, attr, buf);
  if (res >= 0) {
    _devAttrI2c_store(dev, attr, "0", 1);
  }
  return res;
}

static ssize_t _devAttrI2c_store(struct device* dev,
                                 struct device_attribute* attr, const char* buf,
                                 size_t count) {
  struct DeviceAttrRegSpecs* specs;
  struct DeviceAttrBean* dab;
  void* data;
  uint8_t reg;
  int64_t res;

  data = dev_get_drvdata(dev);
  dab = container_of(attr, struct DeviceAttrBean, devAttr);
  if (dab == NULL) {
    return -EFAULT;
  }
  specs = &dab->regSpecsStore;
  if (specs->len == 0) {
    specs = &dab->regSpecs;
    if (specs->len == 0) {
      return -EFAULT;
    }
  }

  res = strToVal(buf, dab->vals, specs->sign, specs->base);
  if (res < 0) {
    return res;
  }

  reg = specs->reg;
  if (data != NULL) {
    reg += I2C_RS485_IDX_TO_REG_START(*((uint8_t*)data));
  }

  res = _i2c_write_segment(reg, specs->len, specs->mask, specs->shift,
                           (uint32_t)res,
                           attr->store != NULL && attr->show != NULL);

  if (res < 0) {
    return res;
  }

  return count;
}

static int getFwVersion(void) {
  int64_t val;
  val = _i2c_read(I2C_REG_FW_VER, 2);

  if (val < 0) {
    return val;
  }

  _fwVerMajor = (val >> 8) & 0xff;
  _fwVerMinor = val & 0xff;

  return 0;
}

static ssize_t _devAttrFwVersion_show(struct device* dev,
                                      struct device_attribute* attr,
                                      char* buf) {
  int32_t res;

  res = getFwVersion();
  if (res < 0) {
    return res;
  }

  return sprintf(buf, "%d.%d\n", _fwVerMajor, _fwVerMinor);
}

static ssize_t _devAttrConfig_store(struct device* dev,
                                    struct device_attribute* attr,
                                    const char* buf, size_t count) {
  int64_t res;
  uint16_t i;
  uint32_t val = toUpper(buf[0]);

  if (!_i2c_trylock()) {
    return -EBUSY;
  }

  res = _i2c_write_no_lock(I2C_REG_USER_CFG_CMD, 2, 0x2a00 | val, 0);
  if (res > 0) {
    for (i = 0; i < 100; i++) {
      msleep(1);
      res = _i2c_read_no_lock(I2C_REG_USER_CFG_STATE, 2);
      if (res > 0) {
        if ((res & 0xff) != val) {
          res = -EBUSY;
          break;
        }
        if (((res >> 10) & 1) == 0) {
          if (((res >> 8) & 1) == 1) {
            res = count;
          } else if (((res >> 11) & 1) == 1) {
            res = -EINVAL;
          } else {
            res = -EFAULT;
          }
          break;
        } else {
          res = -EBUSY;
        }
      }
    }
  }

  _i2c_unlock();

  return res;
}

static ssize_t _devAttrI2cRead_show(struct device* dev,
                                    struct device_attribute* attr, char* buf) {
  if (_i2cReadSize == 1) {
    return sprintf(buf, "0x%02x\n", (uint32_t)_i2cReadVal);
  } else if (_i2cReadSize == 2) {
    return sprintf(buf, "0x%04x\n", (uint32_t)_i2cReadVal);
  } else if (_i2cReadSize == 3) {
    return sprintf(buf, "0x%06x\n", (uint32_t)_i2cReadVal);
  } else {
    return sprintf(buf, "0x%08x\n", (uint32_t)_i2cReadVal);
  }
}

static ssize_t _devAttrI2cRead_store(struct device* dev,
                                     struct device_attribute* attr,
                                     const char* buf, size_t count) {
  unsigned long reg;
  char* end = NULL;

  reg = simple_strtoul(buf, &end, 10);
  if (++end < buf + count) {
    _i2cReadSize = simple_strtoul(end, NULL, 10);
  } else {
    _i2cReadSize = 2;
  }

  if (reg > 255 || _i2cReadSize > 4) {
    return -EINVAL;
  }

  _i2cReadVal = _i2c_read(reg, _i2cReadSize);

  if (_i2cReadVal < 0) {
    return _i2cReadVal;
  }

  return count;
}

static ssize_t _devAttrI2cWrite_store(struct device* dev,
                                      struct device_attribute* attr,
                                      const char* buf, size_t count) {
  unsigned long reg;
  unsigned long val = 0;
  size_t size;
  char* start = NULL;
  char* end = NULL;

  reg = simple_strtoul(buf, &end, 10);

  if (++end >= buf + count) {
    return -EINVAL;
  }

  start = end;
  val = simple_strtoul(start, &end, 16);

  size = (end - start) - 2;

  if (reg > 255 || size > 8 || (size % 2) != 0) {
    return -EINVAL;
  }

  if (_i2c_write(reg, size / 2, val, 0) < 0) {
    return -EIO;
  }

  return count;
}

static ssize_t _devAttrBlink_store(struct device* dev,
                                   struct device_attribute* attr,
                                   const char* buf, size_t count) {
  struct DeviceAttrBean* dab;
  int64_t res;
  uint16_t on = 0;
  uint16_t off = 0;
  uint16_t rep = 0;
  char* end = NULL;

  dab = container_of(attr, struct DeviceAttrBean, devAttr);

  on = simple_strtoul(buf, &end, 10);
  if (++end < buf + count) {
    off = simple_strtoul(end, &end, 10);
    if (++end < buf + count) {
      rep = simple_strtoul(end, NULL, 10);
    }
  }

  res = _i2c_write_segment(dab->regSpecs.reg, dab->regSpecs.len,
                           dab->regSpecs.mask, dab->regSpecs.shift, on, true);
  if (res > 0) {
    res =
        _i2c_write_segment(dab->regSpecs.reg + 1, dab->regSpecs.len,
                           dab->regSpecs.mask, dab->regSpecs.shift, off, true);
    if (res > 0) {
      res = _i2c_write_segment(dab->regSpecs.reg + 2, dab->regSpecs.len,
                               dab->regSpecs.mask, dab->regSpecs.shift, rep,
                               true);
    }
  }

  if (res < 0) {
    return res;
  }

  return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
static int _i2c_probe(struct i2c_client* client) {
#else
static int _i2c_probe(struct i2c_client* client,
                      const struct i2c_device_id* id) {
#endif
  int64_t res;
  bool ok;

  if (strcmp("stratopiplus-rp2", client->name) == 0) {
    _rp2_i2c_client = client;
    _rp2_probed = false;

    if (getFwVersion() == 0) {
      res = _i2c_read(I2C_REG_SYSMON_HW_CFG, 2);
      if (res >= 0) {
        _rp2_probed = true;
        _hwCfg = res;
        dev_info(&client->dev, "HW config: 0x%02hx\n", _hwCfg);
        ok = true;
      }
    }

    if (!_rp2_probed) {
      _rp2_i2c_client = NULL;
      return -1;
    }

  } else {
    dev_err(&client->dev, "unknown i2c_client '%s' addr=0x%02hx\n",
            client->name, client->addr);
    return -1;
  }

  dev_info(&client->dev, "'%s' probed addr=0x%02hx\n", client->name,
           client->addr);
  return 0;
}

static void _i2c_remove(struct i2c_client* client) {
  bool removed = false;

  _i2c_lock();
  if (_rp2_i2c_client == client) {
    _rp2_i2c_client = NULL;
    _rp2_probed = false;
    removed = true;
  }
  _i2c_unlock();

  if (removed) {
    dev_info(&client->dev, "'%s' i2c_client removed addr=0x%02hx\n",
             client->name, client->addr);
  }
}

const struct of_device_id _i2c_of_match[] = {
    {
        .compatible = "sferalabs,stratopiplus-rp2",
    },
    {},
};
MODULE_DEVICE_TABLE(of, _i2c_of_match);

static const struct i2c_device_id _i2c_id[] = {
    {"stratopiplus-rp2", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, _i2c_id);

static struct i2c_driver _i2c_driver = {
    .driver =
        {
            .name = "stratopiplus-i2c",
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(_i2c_of_match),
        },
    .probe = _i2c_probe,
    .remove = _i2c_remove,
    .id_table = _i2c_id,
};

static struct platform_driver stratopiplus_driver;

static int _alloc_minor(void) {
  return ida_alloc_max(&_minor_ida, STRATOPIPLUS_MAX_MINORS - 1, GFP_KERNEL);
}

static void _free_minor(int minor) {
  if (minor >= 0) {
    ida_free(&_minor_ida, minor);
  }
}

static int _device_add(struct platform_device* pdev, struct DeviceBean* db,
                       int idx, void* data) {
  struct device* dev;
  struct DynamicDevice* dd = NULL;
  struct DynamicDevice** pdd;
  struct DeviceAttrBean* dab;
  dev_t devt = 0;
  int minor = -1;
  bool created = false;
  int ai;

  if (idx >= 0) {
    dd = devm_kzalloc(&pdev->dev, sizeof(struct DynamicDevice), GFP_KERNEL);
    if (!dd) {
      return -ENOMEM;
    }
    dd->db = db;
    dd->data = data;
  }
  dev = class_find_device_by_name(_pDeviceClass, db->name);
  if (!dev) {
    minor = _alloc_minor();
    if (minor < 0) {
      pr_err("failed to allocate minor for device '%s' s=%d\n", db->name,
             (idx + 1));
      return minor;
    }
    devt = MKDEV(MAJOR(_devt_base), minor);
    dev = device_create(_pDeviceClass, NULL, devt, dd == NULL ? NULL : dd->data,
                        db->name, (idx + 1));
    if (IS_ERR(dev)) {
      pr_err("failed to create device '%s' s=%d\n", db->name, (idx + 1));
      _free_minor(minor);
      return PTR_ERR(dev);
    }
    created = true;
  } else {
    devt = dev->devt;
    if (devt != 0) {
      minor = MINOR(devt);
    }
  }
  db->device = dev;
  db->devt = devt;
  db->minor = minor;
  if (dd != NULL) {
    dd->device = db->device;
    dd->devt = devt;
    dd->minor = minor;
    pdd = &_dynDevs;
    while (*pdd != NULL) {
      pdd = &(*pdd)->next;
    }
    *pdd = dd;
  }
  ai = 0;
  while (db->devAttrBeans[ai].devAttr.attr.name != NULL) {
    dab = &db->devAttrBeans[ai];
    if (device_create_file(dev, &dab->devAttr)) {
      pr_err("failed to create device file '%s/%s' s=%d\n", db->name,
             dab->devAttr.attr.name, (idx + 1));
      if (created) {
        device_destroy(_pDeviceClass, devt);
        _free_minor(minor);
      }
      return -1;
    }
    ai++;
  }
  return 0;
}

static void _device_remove_files(struct DeviceBean* db, struct device* dev,
                                 dev_t devt, int minor) {
  struct DeviceAttrBean* dab;
  int ai;

  ai = 0;
  while (db->devAttrBeans[ai].devAttr.attr.name != NULL) {
    dab = &db->devAttrBeans[ai];
    device_remove_file(dev, &dab->devAttr);
    ai++;
  }
  device_destroy(_pDeviceClass, devt);
  if (devt != 0) {
    _free_minor(minor);
  }
}

static void cleanup(void) {
  struct DeviceBean* db;
  struct DynamicDevice* dd;
  int di;

  _i2c_lock();
  _rp2_i2c_client = NULL;
  _rp2_probed = false;
  _i2c_unlock();

  if (_pDeviceClass != NULL && !IS_ERR(_pDeviceClass)) {
    di = 0;
    while (_devices[di].name != NULL) {
      db = &_devices[di];
      if (db->device != NULL && !IS_ERR(db->device)) {
        _device_remove_files(db, db->device, db->devt, db->minor);
        db->device = NULL;
        db->devt = 0;
        db->minor = -1;
      }
      di++;
    }

    dd = _dynDevs;
    while (dd != NULL) {
      if (dd->device != NULL) {
        _device_remove_files(dd->db, dd->device, dd->devt, dd->minor);
        dd->device = NULL;
        dd->devt = 0;
        dd->minor = -1;
      }
      dd = dd->next;
    }
    _dynDevs = NULL;
  }
}

static int stratopiplus_init(struct platform_device* pdev) {
  struct DeviceBean* db;
  int i;

  pr_info("init\n");

  if (!_rp2_probed) {
    return -EPROBE_DEFER;
  }

  gpioSetPlatformDev(pdev);

  i = 0;
  while (_devices[i].name != NULL) {
    db = &_devices[i];
    if (_device_add(pdev, db, -1, NULL)) {
      goto fail;
    }
    i++;
  }

  for (i = 0; i < 4; i++) {
    if ((_hwCfg >> i) & 1) {
      if (_device_add(pdev, &_rs485Dev, i, &_rs485DevIdx[i])) {
        goto fail;
      }
    }
  }

  if ((_hwCfg >> 4) & 1) {
    if (_device_add(pdev, &_canDev, 0, NULL)) {
      goto fail;
    }
  }

  if ((_hwCfg >> 5) & 1) {
    if (_device_add(pdev, &_relayDev, 0, NULL)) {
      goto fail;
    }
  }

  pr_info("ready\n");

  return 0;

fail:
  pr_err("init failed\n");
  cleanup();
  return -1;
}

static int __init stratopiplus_module_init(void) {
  int ret;

  if (alloc_chrdev_region(&_devt_base, 0, STRATOPIPLUS_MAX_MINORS,
                          "stratopiplus")) {
    pr_err("failed to allocate chrdev region\n");
    return -ENOMEM;
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
  _pDeviceClass = class_create("stratopiplus");
#else
  _pDeviceClass = class_create(THIS_MODULE, "stratopiplus");
#endif
  if (IS_ERR(_pDeviceClass)) {
    pr_err("failed to create device class\n");
    ret = PTR_ERR(_pDeviceClass);
    _pDeviceClass = NULL;
    goto fail_unregister_chrdev;
  }

  ret = i2c_add_driver(&_i2c_driver);
  if (ret) {
    pr_err("failed to register stratopiplus driver\n");
    goto fail_destroy_class;
  }

  ret = pcf21xx_i2c_register_driver();
  if (ret) {
    pr_err("failed to register PCF2131 driver\n");
    goto fail_del_i2c;
  }

  ret = platform_driver_register(&stratopiplus_driver);
  if (ret) {
    goto fail_unregister_pcf21xx;
  }

  return 0;

fail_unregister_pcf21xx:
  pcf21xx_i2c_unregister_driver();
fail_del_i2c:
  i2c_del_driver(&_i2c_driver);
fail_destroy_class:
  class_destroy(_pDeviceClass);
  _pDeviceClass = NULL;
fail_unregister_chrdev:
  unregister_chrdev_region(_devt_base, STRATOPIPLUS_MAX_MINORS);
  _devt_base = 0;
  return ret;
}

static void __exit stratopiplus_module_exit(void) {
  platform_driver_unregister(&stratopiplus_driver);

  pcf21xx_i2c_unregister_driver();
  i2c_del_driver(&_i2c_driver);

  if (_pDeviceClass != NULL && !IS_ERR(_pDeviceClass)) {
    class_destroy(_pDeviceClass);
    _pDeviceClass = NULL;
  }

  ida_destroy(&_minor_ida);

  unregister_chrdev_region(_devt_base, STRATOPIPLUS_MAX_MINORS);
  _devt_base = 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void stratopiplus_exit(struct platform_device* pdev) {
#else
static int stratopiplus_exit(struct platform_device* pdev) {
#endif
  cleanup();
  pr_info("exit\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
  return 0;
#endif
}

const struct of_device_id stratopiplus_of_match[] = {
    {
        .compatible = "sferalabs,stratopiplus",
    },
    {},
};
MODULE_DEVICE_TABLE(of, stratopiplus_of_match);

static struct platform_driver stratopiplus_driver = {
    .driver =
        {
            .name = "stratopiplus",
            .owner = THIS_MODULE,
            .of_match_table = stratopiplus_of_match,
        },
    .probe = stratopiplus_init,
    .remove = stratopiplus_exit,
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sfera Labs - http://sferalabs.cc");
MODULE_DESCRIPTION("Strato Pi Plus driver module");
MODULE_VERSION(STRATOPIPLUS_MODULE_VERSION);

module_init(stratopiplus_module_init);
module_exit(stratopiplus_module_exit);
