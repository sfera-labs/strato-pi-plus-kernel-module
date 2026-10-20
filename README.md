# Strato Pi Plus driver kernel module

Raspberry Pi OS (Debian) Kernel module for [Strato Pi Plus](https://www.sferalabs.cc/product/strato-pi-plus/).

[![Build tests [stable oldstable]](https://github.com/sfera-labs/strato-pi-plus-kernel-module/actions/workflows/build-apt.yml/badge.svg)](https://github.com/sfera-labs/strato-pi-plus-kernel-module/actions/workflows/build-apt.yml)
[![Build tests [firmware]](https://github.com/sfera-labs/strato-pi-plus-kernel-module/actions/workflows/build-fw.yml/badge.svg)](https://github.com/sfera-labs/strato-pi-plus-kernel-module/actions/workflows/build-fw.yml)

## Installation

Make sure your system is updated:

    sudo apt update
    sudo apt upgrade

If you are using a **32-bit** OS, add to `/boot/firmware/config.txt` (`/boot/config.txt` in older versions) the following line: [[why?](https://github.com/raspberrypi/firmware/issues/1795)]

    arm_64bit=0

Reboot:

    sudo reboot

After reboot, install required tools:

    sudo apt install git device-tree-compiler dkms linux-headers-$(uname -r)

Clone this repo:

    git clone --depth 1 https://github.com/sfera-labs/strato-pi-plus-kernel-module.git
    
    cd strato-pi-plus-kernel-module

### Recommended installation mode: DKMS

This is the recommended mode. It automatically rebuilds and reinstalls the module when new kernel versions are installed.

Set the Raspberry Pi target platform (4 or 5) by writing `stratopiplus-pi4` or `stratopiplus-pi5` in a file named `DTS_NAME`:

    echo stratopiplus-pi4 > DTS_NAME

or

    echo stratopiplus-pi5 > DTS_NAME

Register, build and install with DKMS:

    sudo dkms add .
    sudo dkms build -m $(cat MODULE_NAME) -v $(cat VERSION)
    sudo dkms install -m $(cat MODULE_NAME) -v $(cat VERSION)

### Alternative installation mode: manual install for running kernel only

<details>

<summary>Show</summary>

Use this only if you specifically want to install for the current running kernel version only.

Set the Raspberry Pi target platform (4 or 5) by writing `stratopiplus-pi4` or `stratopiplus-pi5` in a file named `DTS_NAME`:

    echo stratopiplus-pi4 > DTS_NAME

or

    echo stratopiplus-pi5 > DTS_NAME

Then compile and install:

    make clean
    make
    sudo make install

Manual mode does not provide automatic rebuild on kernel upgrades.

</details>

### Enable overlay at boot

Add to `/boot/firmware/config.txt` the following line:

    dtoverlay=stratopiplus

### Optional non-root access to `/sys/class/stratopiplus`

The install process places `99-stratopiplus.rules`, which sets owner group `stratopiplus` for sysfs entries. To access the sysfs interface without superuser privileges, create the group and add your user, e.g. for user "pi":

    sudo groupadd stratopiplus
    sudo usermod -a -G stratopiplus pi

Reboot:

    sudo reboot

## Usage

After installation, the module adds an RTC device and exposes all other Strato Pi Plus features through the sysfs interface in the `/sys/class/stratopiplus/` directory.

### RTC

The RTC seamlessly integrates with the system's standard date and time functionalities.

When an NTP server is available, the system automatically synchronizes the RTC date and time. Conversely, if no network connection is available or the NTP service is disabled, the system relies on the RTC's stored values.  

You can manage and configure the RTC using the `timedatectl` and `hwclock` utilities.

The device node is available at `/dev/rtc0`.

The sysfs interface for the RTC is accessible at `/sys/class/rtc/rtc0`. In addition to the standard RTC attributes, it includes the `battery_low` file. Reading this file returns `1` if the RTC backup battery voltage is low and `0` otherwise.

For advanced usage visit [this page](https://github.com/sfera-labs/rtc-pcf2131#advanced-usage).

### Strato Pi Plus sysfs interface

The sysfs interface provides virtual files that can be read and/or written to, enabling easy monitoring and control of Strato Pi Plus.

Files with the `_config` suffix represent configuration parameters that are saved and retained across power cycles.    
Configuration parameters marked with the `CR` attribute (see the <a href="#attributes">attributes table</a> below) take effect immediately when set. Conversely, parameters with the `C` attribute store the value to be applied during the next *power-up* by copying it to the corresponding "runtime" property (same name without the `_config` suffix).

> [!NOTE]  
> A *power-up* occurs when:
>
> - the unit is powered on (power supply connected);
> - a power cycle is explicitly initiated via `power/down_enabled`; or
> - a power cycle is initiated by another process configured to do so (e.g. watchdog timeout expiration).
>
> A software reboot does **not** correspond to a power-up.

This enables configuring settings for the next power cycles, whether after a planned or abrupt shutdown. For example, it allows switching the boot disk while the Raspberry Pi is off or having a short watchdog timeout during application runtime, which resets to a longer duration after a power cycle to ensure reliable boot and restart.

<a name="attributes"></a>

All properties' attributes are summarized here:

|Attribute|Description|
|:--:|-----------|
|`R`|Readable|
|`W`|Writable|
|`RC`|Readable. Cleared when read|
|`C`|Configuration value, persisted and retained across power cycles, copied to "runtime" counterpart on power-up|
|`CR`|Configuration value, persisted and retained across power cycles, effective immediately|


#### System - `/sys/class/stratopiplus/system/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td>fw_version</td>
            <td>Firmware version</td>
            <td>
                <code>R</code>
            </td>
            <td><i>M</i>.<i>m</i></td>
            <td>Major (<i>M</i>) and minor (<i>m</i>) version</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>config</td>
            <td rowspan=2>Configuration commands</td>
            <td rowspan=2>
                <code>W</code>
            </td>
            <td><code>R</code></td>
            <td>Restore factory default configuration</td>
        </tr>
        <tr>
            <td><code>C</code></td>
            <td>Skip initial Pi power-off on next RP2354 boot (used during firmware update)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>sys_errs</td>
            <td>
                System errors flags.<br/><br/>
                These flags are set as soon as an error occurs and are reset only when read.<br/>
                Upon error, recovery is autonomously attempted, therefore these flags only represent an issue if persistent.
            </td>
            <td>
                <code>RC</code>
            </td>
            <td><i>xIxHxGxFxxxEDCBA</i></td>
            <td>
                Bitmap (0/1) sequence.<br/>
                <i>A</i>: system setup failure<br/>
                <i>B</i>: RP2354 failure<br/>
                <i>C</i>: RP2354 reset occurred<br/>
                <i>D</i>: configuration loading/saving error<br/>
                <i>E</i>: RP2354 I2C slave failure<br/>
                <i>F</i>: RS-485 interface 1 failure<br/>
                <i>G</i>: RS-485 interface 2 failure<br/>
                <i>H</i>: RS-485 interface 3 failure<br/>
                <i>I</i>: RS-485 interface 4 failure<br/>
                <i>x</i>: reserved
            </td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Power cycle - `/sys/class/stratopiplus/power/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=2>down_enabled</td>
            <td rowspan=2>Delayed power cycle enabling</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled. Once enabled cannot be interrupted</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>down_delay_config</td>
            <td>Shutdown delay from enabling</td>
            <td>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>1 ... 65535</td>
            <td>Value in seconds. Default: 60</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>off_time_config</td>
            <td>Power-off duration</td>
            <td>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>1 ... 65535</td>
            <td>Value in seconds. Default: 5</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>up_delay_config</td>
            <td>Power-up delay configuration</td>
            <td>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0 ... 65535</td>
            <td>Value in seconds. Default: 0</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>pcie_switch_config</td>
            <td rowspan=2>PCIe power control configuration on power cycle</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Toggle PCIe power state upon each power cycle</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Watchdog - `/sys/class/stratopiplus/watchdog/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=2>enabled</td>
            <td rowspan=2>Watchdog enabling</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>enabled_config</td>
            <td rowspan=2>Watchdog enabling configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>C</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>timeout</td>
            <td>Watchdog timeout</td>
            <td>
                <code>R</code>
                <code>W</code>
            </td>
            <td>1 ... 65535</td>
            <td>Value in seconds. Default: 60</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>timeout_config</td>
            <td>Watchdog timeout configuration</td>
            <td>
                <code>R</code>
                <code>W</code>
                <code>C</code>
            </td>
            <td>60 ... 65535</td>
            <td>Value in seconds. Default: 60</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>down_delay_config</td>
            <td>Automatic power cycle delay when watchdog expires</td>
            <td>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0 ... 65535</td>
            <td>Value in seconds. Default: 60</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=3>pcie_switch_config</td>
            <td rowspan=3>PCIe power control configuration on watchdog reset</td>
            <td rowspan=3>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Toggling occurs upon each watchdog reset</td>
        </tr>
        <tr>
            <td>2 ... 65535</td>
            <td>Number of consecutive resets (with no heartbeat received in between) before toggling occurs. Requires <code>watchdog/enabled_config</code> set to <code>1</code></td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>heartbeat</td>
            <td>Watchdog heartbeat update</td>
            <td>
                <code>W</code>
            </td>
            <td>1</td>
            <td>Update heartbeat</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>expired</td>
            <td rowspan=2>Watchdog timeout state</td>
            <td rowspan=2>
                <code>R</code>
            </td>
            <td>0</td>
            <td>Not expired</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Expired</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### LED - `/sys/class/stratopiplus/led/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=4>on</td>
            <td rowspan=4>LED control</td>
            <td rowspan=4>
                <code>W</code>
            </td>
            <td>0</td>
            <td>Off</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Steady on</td>
        </tr>
        <tr>
            <td><i>T_ON</i> <i>T_OFF</i></td>
            <td>Continuous blink, <i>T_ON</i> ms on, <i>T_OFF</i> ms off</td>
        </tr>
        <tr>
            <td><i>T_ON</i> <i>T_OFF</i> <i>REPS</i></td>
            <td>Blink <i>REPS</i> times, <i>T_ON</i> ms on, <i>T_OFF</i> ms off</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>sys_pwr_up_config</td>
            <td rowspan=2>Power-up delay state blink sequence configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Don't show</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Show (default)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>sys_pwr_down_config</td>
            <td rowspan=2>Power cycle down delay state blink sequence configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Don't show</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Show (default)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>sys_pwr_off_config</td>
            <td rowspan=2>Power cycle off state blink sequence configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Don't show</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Show (default)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>sys_wd_en_config</td>
            <td rowspan=2>Watchdog enabled state blink sequence configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Don't show (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Show</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>sys_wd_exp_config</td>
            <td rowspan=2>Watchdog timeout expired state blink sequence configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Don't show</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Show (default)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>user_priority_config</td>
            <td rowspan=2>User commands priority configuration for LED</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>System blink sequences can override user commands</td>
        </tr>
        <tr>
            <td>1</td>
            <td>After a user command (write to <code>led/on</code>), system blink sequences are hidden until reset to 0 (default)</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Power Supply Input - `/sys/class/stratopiplus/power_in/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td>mon_v</td>
            <td>Measured input voltage</td>
            <td>
                <code>R</code>
            </td>
            <td><i>V</i></td>
            <td>Value in mV</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>mon_i</td>
            <td>Measured input current</td>
            <td>
                <code>R</code>
            </td>
            <td><i>I</i></td>
            <td>Value in mA</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Fan - `/sys/class/stratopiplus/fan/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td>temp</td>
            <td>Temperature used by fan controller</td>
            <td>
                <code>R</code>
            </td>
            <td><i>T</i></td>
            <td>Value in &deg;C/100 (signed)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>temp_on</td>
            <td>Fan on threshold temperature</td>
            <td>
                <code>R</code>
                <code>W</code>
            </td>
            <td><i>T</i></td>
            <td>Value in &deg;C/100 (signed)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td>temp_off</td>
            <td>Fan off threshold temperature</td>
            <td>
                <code>R</code>
                <code>W</code>
            </td>
            <td><i>T</i></td>
            <td>Value in &deg;C/100 (signed)</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### RS-485 config - `/sys/class/stratopiplus/rs485_s<n>/`

For each detected channel (`<n>` from 1 to 4):

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=4>mode_config</td>
            <td rowspan=4>RS-485 mode configuration</td>
            <td rowspan=4>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Auto (default): if the Raspberry Pi UART is controlling the RTS signal (i.e. it is set low at any time) mode is switched to 2. Otherwise TX-enable (DE line) is controlled by automatically detecting the baud rate and bits settings of the transmitted data</td>
        </tr>
        <tr>
            <td>2</td>
            <td>Passthrough: TX-enable (DE line) mirrors Raspberry Pi UART RTS signal</td>
        </tr>
        <tr>
            <td>3</td>
            <td>Passive: TX-enable (DE line) mirrors Raspberry Pi UART TX signal (this mode requires external pull-up/pull-down on the A/B lines)</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>termination_config</td>
            <td rowspan=2>RS-485 interface line termination configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>inversion_config</td>
            <td rowspan=2>RS-485 interface A/B polarity inversion configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>echo_config</td>
            <td rowspan=2>RS-485 interface local echo configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### CAN - `/sys/class/stratopiplus/can/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=2>termination_config</td>
            <td rowspan=2>CAN bus line termination configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>CR</code>
            </td>
            <td>0</td>
            <td>Disabled (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Relay - `/sys/class/stratopiplus/relay/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=2>status</td>
            <td rowspan=2>Relay output state</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
            </td>
            <td>0</td>
            <td>Rest position (default)</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Switched position</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### Secure Element - `/sys/class/stratopiplus/sec_elem/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td>serial_num</td>
            <td>ATECC secure element serial number</td>
            <td>
                <code>R</code>
            </td>
            <td><i>hex string</i></td>
            <td>Device unique identifier</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>

#### PCIe interface - `/sys/class/stratopiplus/pcie/`

<table>
    <thead>
        <tr>
            <th>File</th>
            <th>Description</th>
            <th><a href="#attributes">Attr</a></th>
            <th>Value</th>
            <th>Value description</th>
        </tr>
    </thead>
    <!-- ================= -->
    <tbody>
        <tr>
            <td rowspan=2>enabled</td>
            <td rowspan=2>PCIe interface enabling</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled</td>
        </tr>
        <!-- ------------- -->
        <tr>
            <td rowspan=2>enabled_config</td>
            <td rowspan=2>PCIe interface enabling configuration</td>
            <td rowspan=2>
                <code>R</code>
                <code>W</code>
                <code>C</code>
            </td>
            <td>0</td>
            <td>Disabled</td>
        </tr>
        <tr>
            <td>1</td>
            <td>Enabled (default)</td>
        </tr>
        <!-- ------------- -->
    </tbody>
</table>
