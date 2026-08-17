# 01 — AM3352 SoC 與 am3352-evm 的核心開機路徑

**What to build:** 讓使用者能執行 `qemu-system-arm -M am3352-evm -kernel <zImage> -dtb <am3352-evm.dtb>`，看到主線 Linux kernel 開機訊息並抵達 console。範圍只需要能支援 console 開機的必要配置：CPU、RAM、UART0、DMTimer2、INTC、PRCM/Control Module 簡化 stub，以及一份跟這些裝置位址一致的精簡裝置樹。其餘缺件（未實作的周邊）驅動探測失敗時應優雅跳過，不阻塞開機。

**Blocked by:** None — can start immediately

**Status:** ready-for-agent

- [ ] `hw/arm/am3352.c`／`include/hw/arm/am3352.h` 建立 SoC 容器：`cortex-a8` CPU、`am335x_intc`、`am335x_timer`（DMTimer2 @ `0x48040000`）、`serial_mm_init()` 建立的 UART0（`ns16550a` 相容 @ `0x44E09000`）、`TYPE_UNIMPLEMENTED_DEVICE` 覆蓋 PRCM（`CM_WKUP` @ `0x44E00400`、`CM_PER` @ `0x44E00000`）與 Control Module（@ `0x44E10000`），IRQ 依 spec 記憶體映射表接妥
- [ ] `hw/intc/am335x_intc.c`／`include/hw/intc/am335x_intc.h` 新增相容 `ti,am33xx-intc` 的中斷控制器（@ `0x48200000`），至少實作 `INTC_REVISION`、`SYSCONFIG`/`SYSSTATUS`、`SIR_IRQ`、`CONTROL`、`MIR_CLEAR0-2`/`MIR_SET0-2`、`ITR0-2`、`PENDING_IRQ0-2`，輸出接到 CPU 的 IRQ/FIQ 輸入
- [ ] `hw/timer/am335x_timer.c`／`include/hw/timer/am335x_timer.h` 新增相容 `ti,am335x-timer` 的計時器，至少實作 `TIDR`、`TIOCP_CFG`、`IRQSTATUS`/`IRQSTATUS_SET`/`IRQSTATUS_CLR`/`IRQENABLE_SET`/`IRQENABLE_CLR`、`TCLR`、`TCRR`、`TLDR`、`TTGR`
- [ ] `hw/arm/am3352-evm.c` 註冊 `am3352-evm` `MachineClass`（預設 RAM 256 MiB @ `0x80000000`），透過 `arm_load_kernel()` 支援 `-kernel`/`-dtb`/`-initrd`，不設定 `get_dtb`
- [ ] `hw/arm/Kconfig` 新增 `AM3352_EVM`，`meson.build` 加入四個新原始檔，`ninja` 能成功建出含 `am3352-evm` 機型的 `qemu-system-arm`
- [ ] `tests/functional/arm/am3352-evm.dts`（或等效位置）撰寫精簡裝置樹，只含 UART0/DMTimer2/INTC/memory 節點，位址與上面的 QEMU 實作一致，能用 `dtc` 編譯成 `.dtb`
- [ ] 手動驗證：用真實 armhf 發行版 kernel + 上述 dtb 開機，PRCM/Control Module 探測不卡住、不觸發 bus fault，開機訊息能透過 UART0 印出、抵達 console 或至少看到 kernel 完整跑完 init（若因為缺少 rootfs 卡在 VFS mount，視為本項已達成，第 02 項的 functional test 再補上完整 initrd 驗證）
