#!/usr/bin/env python3
#
# Functional test that boots Linux on a Raspberry Pi 5 machine
#
# SPDX-License-Identifier: GPL-2.0-or-later

from qemu_test import Asset, LinuxKernelTest
from qemu_test import exec_command_and_wait_for_pattern


class Aarch64Raspi5Machine(LinuxKernelTest):

    timeout = 360

    ASSET_KERNEL = Asset(
        ('https://deb.debian.org/debian/dists/stable/main/installer-arm64/'
         '20250803+deb13u6/images/netboot/debian-installer/arm64/linux'),
        'cbe59a02e7ea979a150661032440c94e2c4db0b735af2416e11ae5cac15a58e4')

    ASSET_DTB = Asset(
        ('https://deb.debian.org/debian/dists/stable/main/installer-arm64/'
         '20250803+deb13u6/images/device-tree/broadcom/'
         'bcm2712-rpi-5-b.dtb'),
        'b8f261ed405560ad2049a14dfaa80ed235188e42089daf763328763b0b70ea0f')

    ASSET_INITRD = Asset(
        ('https://github.com/groeck/linux-build-test/raw/'
         '86b2be1384d41c8c388e63078a847f1e1c4cb1de/rootfs/'
         'arm64/rootfs.cpio.gz'),
        '7c0b16d1853772f6f4c3ca63e789b3b9ff4936efac9c8a01fb0c98c05c7a7648')

    def test_arm_raspi5_initrd(self):
        self.set_machine('raspi5b')
        self.vm.set_console()
        self.vm.add_args(
            '-kernel', self.ASSET_KERNEL.fetch(),
            '-dtb', self.ASSET_DTB.fetch(),
            '-initrd', self.uncompress(self.ASSET_INITRD),
            '-append', (self.KERNEL_COMMON_COMMAND_LINE +
                        'earlycon=pl011,mmio32,0x107d001000 '
                        'console=ttyAMA10,115200 rdinit=/bin/sh panic=-1'),
            '-no-reboot')
        self.vm.launch()

        self.wait_for_console_pattern(
            'SMP: Total of 4 processors activated.')
        self.wait_for_console_pattern('~ #')
        exec_command_and_wait_for_pattern(
            self, 'echo raspi5-functional-test', 'raspi5-functional-test')


if __name__ == '__main__':
    LinuxKernelTest.main()
