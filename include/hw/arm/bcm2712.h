/*
 * BCM2712 SoC emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_ARM_BCM2712_H
#define HW_ARM_BCM2712_H

#include "hw/arm/bcm2712_peripherals.h"
#include "hw/intc/arm_gic.h"
#include "target/arm/cpu.h"

#define TYPE_BCM2712 "bcm2712"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2712State, BCM2712)

#define BCM2712_NUM_CPUS 4

struct BCM2712State {
    SysBusDevice parent_obj;
    ARMCPU cpu[BCM2712_NUM_CPUS];
    GICState gic;
    BCM2712PeripheralState peripherals;
};

#endif
