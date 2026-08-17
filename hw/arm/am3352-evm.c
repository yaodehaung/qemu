/*
 * am3352-evm: a generic TI AM3352 development board.
 *
 * This does not model any specific real product (TI's AM335x EVM,
 * BeagleBone, or otherwise) — it is the minimal AM3352 hardware set
 * needed to boot an unmodified mainline Linux kernel to a console. See
 * docs/am3352-support-spec.md.
 *
 * Only -kernel/-dtb (and optionally -initrd) boot is supported: there is
 * no boot ROM, U-Boot/SPL, or NAND/eMMC/SD boot path.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/am3352.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/core/boards.h"
#include "system/address-spaces.h"
#include "system/qtest.h"
#include "qemu/units.h"

static struct arm_boot_info am3352_evm_binfo = {
    .loader_start = AM3352_SDRAM_ADDR,
    .board_id = -1, /* device-tree-only board */
};

static void am3352_evm_init(MachineState *machine)
{
    AM3352State *soc = g_new0(AM3352State, 1);

    object_initialize_child(OBJECT(machine), "soc", soc, TYPE_AM3352);
    qdev_realize(DEVICE(soc), NULL, &error_fatal);

    memory_region_add_subregion(get_system_memory(), AM3352_SDRAM_ADDR,
                                machine->ram);

    am3352_evm_binfo.ram_size = machine->ram_size;

    /*
     * As with other boards that don't synthesize their own device tree
     * (get_dtb is left unset), the user supplies -dtb; there is no
     * upstream am33xx.dtsi that matches this reduced device set.
     */
    if (!qtest_enabled()) {
        arm_load_kernel(&soc->cpu, machine, &am3352_evm_binfo);
    }
}

static void am3352_evm_machine_init(MachineClass *mc)
{
    mc->desc = "TI AM3352 generic development board (Cortex-A8)";
    mc->init = am3352_evm_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_ram_size = 256 * MiB;
    mc->default_ram_id = "am3352-evm.ram";
    mc->default_cpus = mc->min_cpus = mc->max_cpus = 1;
}

DEFINE_MACHINE_ARM("am3352-evm", am3352_evm_machine_init)
