PC/SC Pass Through Linux Driver
======

This project is a side project of GlobalPlatform Secure Element API implementation in [OP-TEE].

Goal of this project is to provide a smartcard simulation framework on-top of QEMU, it coperates with the following [PC/SC Lite] and [vsmartcard] to accomplish this feature.

- **PC/SC Lite** is a PC/SC runtime environment that enables client application access to smartcard through a standard API.
- **vsmartcard** is a smartcard simulation framework written in Python. It consist of two components:
 - **vpcd**: a PC/SC Lite reader driver that implements the low-level reader operations.
 - **vicc**: the simulator itself, it is connected with vpcd through socket.

The idea is to implement a simple QEMU device that act as PC/SC client, it provides a **simple sw/hw interface** that can be used to by-pass PC/SC operations through this device. The driver is used to support this device, as you expected.

![](https://docs.google.com/drawings/d/16mYZDc1jPuna_Vjr6kQnhd1yfxoECoCzcwiLQ9bZMVk/pub?w=711&h=701)

Installation
======
You need to get a modified version of [QEMU]

```sh
# install PC/SC runtime and library
sudo apt-get install pcscd libpcsclite-dev

git clone https://github.com/m943040028/qemu.git
cd qemu
git checkout -b smart_card_emul origin/smart_card_emul
./configure --target-list="arm-softmmu" --enable-pcsc-passthru
make; sudo make install
```

Get [linaro-stable-kernel]. By default, it adds a device to ARM Vexpress platform. You need to add the following section to device file(**arch/arm/boot/dts/vexpress-v2p-ca15-tc1.dts**) to describe this device.

```c
pcsc@0x1f000000 {
   compatible = "linaro,pcsc-passthru";
   reg = <0 0x1f000000 0 0x1000>;
   interrupts = <0 47 4>;
};
```

Get [vsmartcard] and install it

```sh
git clone https://github.com/frankmorgner/vsmartcard.git
cd vsmartcard/virtualsmartcard

./configure --sysconfdir=/etc
make; sudo make install
```

Finally, build the driver

```sh
DRV=`pwd`
cd path/to/linux-linaro-stable
make V=0  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=$DRV modules
```

Testing
======

Make sure you have **vpcd** installed and configured. Make sure the file **/etc/reader.conf.d/vpcd** is existed. (It should be installed when you install [vsmartcard])

Run **vicc**, after this command, a virtual smartcard is inerted to Reader.0

```sh
vicc -t iso7816 -v
```

Prepare a linux kernel image with minimal rootfs and this driver installed. Run it with QEMU

```sh
qemu-system-arm -kernel path/to/zImage -M vexpress-a15 -dtb path/to/vexpress-v2p-ca15-tc1.dtb \
-m 1024 -append 'console=ttyAMA0,38400n8 init=/linuxrc' -serial stdio
```

You can append **-d guest_errors -D /tmp/qemu.log** for debugging. Insert the driver and you can see 10 readers detected

```sh
root@Vexpress:/ insmod /lib/modules/3.10.50/pcsc_passthru.ko 
number of reader detected: 10
```

You can check the state of Reader.0 via

```sh
root@Vexpress:/ cd sys/devices/platform/pcsc_reader.0
root@Vexpress:/sys/devices/platform/pcsc_reader.0 cat state
Reader State:
  Card inserted
Card Connected: [No]
```

You can create a connection to the Virtual Card in the reader

```sh
root@Vexpress:/sys/devices/platform/pcsc_reader.0 echo 1 > connect
```

Check the card state again, you can see the card is connected, and ATR([ISO/IEC7816-3]) is retrieved

```sh
root@Vexpress:/sys/devices/platform/pcsc_reader.0 cat state
Reader State:
  Card inserted
  Shared Mode
Card Connected: [Yes]
ATR: 3B 95 13 81 01 80 73 FF 01 00 0B
```
Upon connected, you may start to trasmit APDU([ISO/IEC7816-4]), here is an example

```sh
root@Vexpress:/sys/devices/platform/pcsc_reader.0 echo "00 A4 00 00 02 3F 00" > transmit 
platform pcsc_reader.0: Received: 6F 0A 83 02 3F 00 8A 01 05 82 01 38 61 0C
```

To test card removal, kill the **vicc** and check it again

```sh
root@Vexpress:/sys/devices/platform/pcsc_reader.0 cat state
Reader State:
  Card removed

```

[OP-TEE]: https://github.com/OP-TEE
[QEMU]: https://github.com/m943040028/qemu/tree/smart_card_emul
[PC/SC Lite]: http://pcsclite.alioth.debian.org/pcsclite.html
[vsmartcard]: http://frankmorgner.github.io/vsmartcard/virtualsmartcard/README.html
[linaro-stable-kernel]: https://git.linaro.org/?p=kernel/linux-linaro-stable.git
[ISO/IEC7816-3]:http://read.pudn.com/downloads132/doc/comm/563504/ISO-IEC%207816/ISO%2BIEC%207816-3-2006.pdf
[ISO/IEC7816-4]:http://www.embedx.com/pdfs/ISO_STD_7816/info_isoiec7816-4%7Bed2.0%7Den.pdf

