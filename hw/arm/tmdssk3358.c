/*
 * tmdssk3358: TI's AM335x Starter Kit (TMDSSK3358).
 *
 * This is a minimal boot-path model, not a hardware-accurate one: it only
 * implements the AM3358 hardware set needed to boot an unmodified mainline
 * Linux kernel to a console (CPU, RAM, UART, DMTimer, INTC, and
 * unimplemented-device stubs for PRCM/Control Module). The real
 * TMDSSK3358 board's 4.3" touchscreen LCD, dual Gb Ethernet switch,
 * WiFi/Bluetooth, and TPS65910 power-management IC are not modelled. See
 * docs/tmdssk3358-support-spec.md.
 *
 * Only -kernel/-dtb (and optionally -initrd) boot is supported: there is
 * no boot ROM, U-Boot/SPL, or NAND/eMMC/SD boot path.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/am3358.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/core/boards.h"
#include "system/address-spaces.h"
#include "system/qtest.h"
#include "qemu/units.h"

static struct arm_boot_info tmdssk3358_binfo = {
    .loader_start = AM3358_SDRAM_ADDR,
    .board_id = -1, /* device-tree-only board */
};

static void tmdssk3358_init(MachineState *machine)
{
    AM3358State *soc = g_new0(AM3358State, 1);

    object_initialize_child(OBJECT(machine), "soc", soc, TYPE_AM3358);
    qdev_realize(DEVICE(soc), NULL, &error_fatal);

    memory_region_add_subregion(get_system_memory(), AM3358_SDRAM_ADDR,
                                machine->ram);

    tmdssk3358_binfo.ram_size = machine->ram_size;

    /*
     * As with other boards that don't synthesize their own device tree
     * (get_dtb is left unset), the user supplies -dtb; there is no
     * upstream am33xx.dtsi that matches this reduced device set.
     */
    if (!qtest_enabled()) {
        arm_load_kernel(&soc->cpu, machine, &tmdssk3358_binfo);
    }
}

static void tmdssk3358_machine_init(MachineClass *mc)
{
    mc->desc = "TI TMDSSK3358 AM335x Starter Kit, minimal boot-path model (Cortex-A8)";
    mc->init = tmdssk3358_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_ram_size = 256 * MiB;
    mc->default_ram_id = "tmdssk3358.ram";
    mc->default_cpus = mc->min_cpus = mc->max_cpus = 1;
}

DEFINE_MACHINE_ARM("tmdssk3358", tmdssk3358_machine_init)
