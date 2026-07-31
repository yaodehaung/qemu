/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef Q32_CPU_QOM_H
#define Q32_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_Q32_CPU "q32-cpu"
OBJECT_DECLARE_CPU_TYPE(Q32CPU, Q32CPUClass, Q32_CPU)

#define Q32_CPU_TYPE_SUFFIX "-" TYPE_Q32_CPU
#define Q32_CPU_TYPE_NAME(model) model Q32_CPU_TYPE_SUFFIX

#endif
