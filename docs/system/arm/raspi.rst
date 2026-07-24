Raspberry Pi boards
==================


QEMU provides models of the following Raspberry Pi boards:

``raspi0`` and ``raspi1ap``
  ARM1176JZF-S core, 512 MiB of RAM
``raspi2b``
  Cortex-A7 (4 cores), 1 GiB of RAM
``raspi3ap``
  Cortex-A53 (4 cores), 512 MiB of RAM
``raspi3b``
  Cortex-A53 (4 cores), 1 GiB of RAM
``raspi4b``
  Cortex-A72 (4 cores), 2 GiB of RAM
``raspi5b``
  Cortex-A76 (4 cores), 640 MiB of RAM

Implemented devices
-------------------

 * ARM1176JZF-S, Cortex-A7, Cortex-A53, Cortex-A72 or Cortex-A76 CPU
 * Interrupt controller
 * DMA controller
 * Clock and reset controller (CPRMAN)
 * System Timer
 * GPIO controller
 * Serial ports (BCM2835 AUX - 16550 based - and PL011)
 * Random Number Generator (RNG)
 * Frame Buffer
 * USB host (USBH)
 * GPIO controller
 * SD/MMC host controller
 * SoC thermal sensor
 * USB2 host controller (DWC2 and MPHI)
 * MailBox controller (MBOX)
 * VideoCore firmware (property)
 * Peripheral SPI controller (SPI)
 * Broadcom Serial Controller (I2C)

The ``raspi5b`` machine intentionally implements a smaller boot-oriented set:

 * Cortex-A76 CPUs (4 cores)
 * GIC-400 interrupt controller
 * BCM2835-compatible system timer
 * PL011 debug UART
 * BCM2712-compatible SDHCI controller

Booting ``raspi5b``
-------------------

The machine boots a 64-bit Linux kernel directly.  A typical invocation is::

  qemu-system-aarch64 -M raspi5b \
    -kernel kernel8.img \
    -dtb bcm2712-rpi-5-b.dtb \
    -initrd rootfs.cpio.gz \
    -append "earlycon=pl011,mmio32,0x107d001000 console=ttyAMA10,115200" \
    -serial mon:stdio

QEMU does not emulate the Raspberry Pi firmware boot flow for this machine,
so the kernel and device tree must be supplied explicitly.  A minimal mainline
Raspberry Pi 5 device tree is recommended because the machine does not
implement RP1 or the other devices listed below.

Missing devices
---------------

 * Pulse Width Modulation (PWM)
 * PCIE Root Port (raspi4b)
 * GENET Ethernet Controller (raspi4b)
 * RP1 south bridge (raspi5b)
 * PCIE Root Port (raspi5b)
 * GPIO controller (raspi5b)
 * USB controllers (raspi5b)
 * Ethernet controller (raspi5b)
 * VideoCore/GPU and firmware mailbox (raspi5b)
 * I2C and SPI controllers (raspi5b)
 * SoC thermal sensor (raspi5b)
 * Random Number Generator (raspi5b)
