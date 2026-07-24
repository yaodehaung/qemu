/*
 * BCM2712 minimal peripheral emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_ARM_BCM2712_PERIPHERALS_H
#define HW_ARM_BCM2712_PERIPHERALS_H

#include "hw/char/pl011.h"
#include "hw/sd/sdhci.h"
#include "hw/timer/bcm2835_systmr.h"

#define TYPE_BCM2712_PERIPHERALS "bcm2712-peripherals"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2712PeripheralState, BCM2712_PERIPHERALS)

struct BCM2712PeripheralState {
    SysBusDevice parent_obj;
    PL011State uart;
    BCM2835SystemTimerState systimer;
    SDHCIState sdhci;
};

#endif
