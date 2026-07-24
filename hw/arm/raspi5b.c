/*
 * Raspberry Pi 5B emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "hw/arm/bcm2712.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/arm/raspi_platform.h"
#include "hw/core/boards.h"
#include "hw/core/qdev-properties.h"
#include "hw/sd/sd.h"
#include "system/block-backend.h"
#include "system/blockdev.h"

#define TYPE_RASPI5B_MACHINE MACHINE_TYPE_NAME("raspi5b")
OBJECT_DECLARE_SIMPLE_TYPE(Raspi5bMachineState, RASPI5B_MACHINE)

#define RASPI5B_RAM_SIZE (640 * MiB)

struct Raspi5bMachineState {
    RaspiBaseMachineState parent_obj;
    BCM2712State soc;
};

static void raspi5b_machine_init(MachineState *machine)
{
    Raspi5bMachineState *s = RASPI5B_MACHINE(machine);
    RaspiBaseMachineState *s_base = RASPI_BASE_MACHINE(machine);
    DriveInfo *di;
    BlockBackend *blk;
    DeviceState *card;

    memory_region_add_subregion(get_system_memory(), 0, machine->ram);

    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_BCM2712);
    sysbus_realize(SYS_BUS_DEVICE(&s->soc), &error_fatal);

    di = drive_get(IF_SD, 0, 0);
    blk = di ? blk_by_legacy_dinfo(di) : NULL;
    card = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_drive_err(card, "drive", blk, &error_fatal);
    qdev_realize_and_unref(card,
                           qdev_get_child_bus(DEVICE(&s->soc.peripherals),
                                              "sd-bus"),
                           &error_fatal);

    s_base->binfo.ram_size = machine->ram_size;
    s_base->binfo.loader_start = 0;
    s_base->binfo.psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    arm_load_kernel(&s->soc.cpu[0], machine, &s_base->binfo);
}

static void raspi5b_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Raspberry Pi 5 Model B";
    mc->init = raspi5b_machine_init;
    mc->block_default_type = IF_SD;
    mc->auto_create_sdcard = true;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->default_cpus = mc->min_cpus = mc->max_cpus = BCM2712_NUM_CPUS;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a76");
    mc->default_ram_size = RASPI5B_RAM_SIZE;
    mc->default_ram_id = "ram";
}

static const TypeInfo raspi5b_machine_type = {
    .name = TYPE_RASPI5B_MACHINE,
    .parent = TYPE_RASPI_BASE_MACHINE,
    .instance_size = sizeof(Raspi5bMachineState),
    .class_init = raspi5b_machine_class_init,
    .interfaces = aarch64_machine_interfaces,
};

static void raspi5b_machine_register_type(void)
{
    type_register_static(&raspi5b_machine_type);
}

type_init(raspi5b_machine_register_type)
