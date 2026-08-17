# 規格書：QEMU 上的 TI TMDSSK3358（AM3358 Starter Kit）最小開機支援

狀態：ready-for-agent
負責人：nickhuang
目標：QEMU 上游主線（`qemu-devel`，hw/arm maintainer：Peter Maydell。`MAINTAINERS` 目前只有舊版 OMAP1 的 `OMAP` 條目，標記 `Odd Fixes`，AM335x 尚無專屬維護者）

## 問題陳述

QEMU 目前完全沒有 TI Sitara AM335x/AM3358 系列 SoC 的模擬支援（已確認 `hw/arm/`、`target/arm/` 下都沒有相關程式碼）。現有最接近的既有裝置是 `hw/*/omap*.c`，但那是 OMAP1（ARM926）世代的暫存器與架構，跟 AM3358（Cortex-A8、OMAP3 周邊世代）不相容，不能重用。想要在沒有實體 TI Sitara 硬體時開發、測試 am335x 相關 kernel 驅動或韌體，或是需要 CI pipeline 跑 am335x 開機驗證的人，現在完全無法在 QEMU 底下做這件事。

## 解決方案

新增一個 QEMU 機型 `tmdssk3358`：TI 官方 **AM3358 Starter Kit（TMDSSK3358）** 的最小開機子集模型（不是硬體精確的完整模型），模擬能讓**不修改的主線 Linux kernel**開機到 console 提示符所需的最小 AM3358 硬體集合，包括 CPU、RAM、一顆 UART（console）、一顆相容 `ti,am335x-timer` 的 DMTimer、一顆相容 `ti,am33xx-intc` 的中斷控制器，以及涵蓋 PRCM／Control Module 位址範圍的簡化 stub。真實 TMDSSK3358 板子上的 4.3" 觸控 LCD、雙 Gb Ethernet switch、WiFi/藍牙、TPS65910 電源管理 IC 均未模擬。開機流程沿用 QEMU 既有 ARM 機型常見的 `-kernel`/`-dtb`（可選 `-initrd`）工作流程，不模擬 boot ROM、U-Boot/SPL，或 NAND/eMMC/SD 開機路徑。

因為周邊集合是本案自行決定的最小子集，位址上雖然沿用真實 AM3358 TRM 的記憶體映射（降低與真實驅動的落差、方便日後擴充），但**不會**對應真實硬體的完整裝置樹。裝置樹（dtb）因此由本案自行撰寫並隨測試素材維護，而不是使用上游 `am33xx.dtsi`／BeagleBone dtb。

## 需求

核心情境：沒有實體 TI Sitara 硬體、想開發或測試 am335x kernel 驅動的人，能用 `qemu-system-arm -M tmdssk3358 -kernel <kernel> -dtb <dtb>`（可選 `-initrd`）把一份未修改的 Linux kernel 開到 console，命令列跟其他採用相同 `-kernel`/`-dtb`/`-initrd` 慣例的 QEMU ARM 機型一致，不用另外學新用法。CPU 用 `cortex-a8`（單核心；`target/arm/tcg/cpu32-system.c` 已有現成 model，不需要新增），UART 位址對應真實 AM3358 的 UART0，中斷經模擬的 `ti,am33xx-intc` 正確路由到 UART 與計時器，計時器相容 `ti,am335x-timer`，同時充當 clocksource 與 clockevent。

主線 Linux 的 am33xx clock（CCF）與 `pinctrl-single` 驅動在探測 PRCM／Control Module 時不能讓開機卡住或觸發 bus fault，即使這兩個模組本身沒有被完整模擬；其餘沒實作的周邊（乙太網路、MMC/SD、I2C、SPI、PWM、ADC、RTC、看門狗……）驅動探測失敗時要能優雅跳過。裝置樹隨測試素材維護、位址跟 QEMU 端實作保持一致，這樣才能直接拿現成 dtb 開機，不用自己拼裝置樹。

對 reviewer 跟未來維護者：程式碼組織比照現有簡單單核 Cortex-A8 SoC 的先例（`hw/arm/fsl-imx25.c` + `hw/arm/imx25_pdk.c` 的 SoC／機型雙檔分離），`MAINTAINERS`、`docs/system/arm/`、Kconfig/meson.build 都要跟著更新，並附上一個 functional test 避免合併後悄悄出現迴歸。機型文件要寫清楚這是 TMDSSK3358 的最小開機子集模型、非硬體精確、不支援 U-Boot，以及目前已實作/缺少的裝置清單。

## 實作決策

- **新機型**：`tmdssk3358`，仿照 `imx25_pdk`（單顆簡單 Cortex-A8/ARM926 級 SoC + 精簡機型）的分檔模式建模，而非 `bcm2712`/`bcm2838` 那種「SoC + 獨立 peripherals 容器」三檔模式。本案周邊數量小，三檔分離不成比例。
- **新增原始碼檔案**：
  - `hw/arm/am3358.c`（+ `include/hw/arm/am3358.h`）：SoC 容器物件（對應 `hw/arm/fsl-imx25.c`），建立 `cortex-a8` CPU、中斷控制器、計時器、UART、PRCM/Control Module stub，接進位址空間、連接 IRQ 線路。
  - `hw/arm/tmdssk3358.c`：機型定義（對應 `hw/arm/imx25_pdk.c`），註冊 `tmdssk3358` 這個 `MachineClass`，設定預設 RAM 256 MiB、CPU 型號，呼叫 `hw/arm/boot.c` 的 `arm_load_kernel()` 處理 `-kernel`/`-dtb`/`-initrd`；機型程式碼**不會**設定 `get_dtb` callback，使用者需自備（或使用本案隨測試素材提供的）dtb。
  - `hw/intc/am335x_intc.c`（+ `include/hw/intc/am335x_intc.h`）：新裝置，相容 `ti,am33xx-intc`。只完整實作驅動實際會用到的暫存器子集：`INTC_REVISION`、`INTC_SYSCONFIG`/`SYSSTATUS`、`INTC_SIR_IRQ`、`INTC_CONTROL`、`INTC_MIR_CLEAR0-2`/`MIR_SET0-2`、`INTC_ITR0-2`、`INTC_PENDING_IRQ0-2`；FIQ／protection／idle 相關暫存器先 RAZ/WI，缺件視驅動實測結果補齊。
  - `hw/timer/am335x_timer.c`（+ `include/hw/timer/am335x_timer.h`）：新裝置，相容 `ti,am335x-timer`。只實作驅動路徑真正用到的暫存器：`TIDR`、`TIOCP_CFG`、`IRQSTATUS`/`IRQSTATUS_SET`/`IRQSTATUS_CLR`/`IRQENABLE_SET`/`IRQENABLE_CLR`、`TCLR`、`TCRR`、`TLDR`、`TTGR`。單一實例同時充當 clocksource 與 clockevent 來源，不另外實作第二顆計時器。
  - PRCM/Control Module：不新增自訂裝置程式碼，直接在 `am3358.c` 的 SoC 容器裡用 QEMU 既有的 `TYPE_UNIMPLEMENTED_DEVICE`（`hw/misc/unimplemented-device.c`）分別覆蓋 PRCM（`CM_PER`/`CM_WKUP`）與 Control Module（pinmux）位址範圍。`unimplemented-device` 對所有讀取回傳 `0`；因為 am33xx clock 驅動判斷模組時鐘就緒的 `IDLEST` 欄位其「功能正常」值恰好是 `0b00`，回傳全零天然滿足這個輪詢條件，不需要另外寫狀態機。**風險**：如果實測發現 clk 驅動或 `pinctrl-single` 對某些欄位需要非零值才視為就緒，屆時再補一顆專用的小型 stub 裝置（見補充說明）。
  - UART：不寫新裝置，直接重用既有的 `hw/char/serial.c`（16550 相容通用序列埠），透過 `serial_mm_init()` 掛進位址空間。裝置樹節點的 `compatible` 使用 `"ns16550a"`（一般 8250 驅動），刻意不用 `"ti,am3358-uart"`（會 bind `8250_omap` 驅動，該驅動會主動操作 TI 專屬的 `UART_OMAP_MDR1` 模式暫存器，而 `serial.c` 沒有實作這顆暫存器）。這是本案在「自寫精簡 dtb」的自由度下選擇的較低風險路徑：`ns16550a` 仍是真實、未修改的主線驅動，只是換一條更簡單、QEMU 已有裝置就能滿足的驅動路徑。
- **CPU**：`cortex-a8`，單核心。已在 `target/arm/tcg/cpu32-system.c` 實作完成，不需要新的 CPU model 工作。
- **記憶體映射表**（沿用真實 AM3358 TRM 的位址，即使只實作子集，維持位址配置的可信度、方便日後擴充）：
  - UART0（console）：`0x44E09000`。
  - INTC（MPU INTC）：`0x48200000`。
  - DMTimer2：`0x48040000`（clocksource + clockevent 共用）。
  - PRCM CM_WKUP：`0x44E00400`；CM_PER：`0x44E00000`（`unimplemented-device` stub）。
  - Control Module（pinmux）：`0x44E10000`（`unimplemented-device` stub）。
  - RAM：從 `0x80000000` 起，預設 256 MiB。
- **開機機制**：透過 `arm_load_kernel()`（`hw/arm/boot.c`）處理 `-kernel`/`-dtb`/`-initrd`；只支援直接 kernel+dtb 開機，不模擬 boot ROM、U-Boot/SPL，也不支援從模擬的 NAND/eMMC/SD 開機。
- **裝置樹**：自行撰寫一份精簡 `.dts`（隨測試素材維護，例如 `tests/functional/arm/tmdssk3358.dts`，測試執行時用 `dtc` 編譯成 `.dtb`），只描述上述五項已實作裝置＋`memory` 節點，不含真實 AM3358 硬體上其他任何裝置節點。
- **中斷路由**：UART0 與 DMTimer2 的 IRQ 依真實 AM3358 TRM 的中斷編號接進 `am335x_intc`；INTC 的 IRQ 輸出接到 CPU 的 IRQ/FIQ 輸入（AM3358 無 GIC，MPU 直接吃 INTC 的 nIRQ/nFIQ）。
- **Kconfig / build 接線**：新增 `hw/arm/Kconfig` 的 `config TMDSSK3358`，`select ARM_V7`、`select SERIAL_MM`、新裝置對應的 select 項；把 `am3358.c`、`tmdssk3358.c`、`am335x_intc.c`、`am335x_timer.c` 加進對應的 `meson.build`。
- **MAINTAINERS**：新增一段 `AM3358` 條目，`F: hw/arm/am3358.c`、`F: hw/arm/tmdssk3358.c`、`F: hw/intc/am335x_intc.c`、`F: hw/timer/am335x_timer.c`、`F: include/hw/arm/am3358.h` 等，狀態標 `Odd Fixes` 或掛在既有 `ARM` 一般條目下（沿用既有 `OMAP` 條目模式，因為目前沒有專屬維護者）。
- **文件**：新增 `docs/system/arm/tmdssk3358.rst`，格式比照 `docs/system/arm/cubieboard.rst`，列出 emulated devices（Timer、UART、INTC），並明確加註「這是 TMDSSK3358 的最小開機子集模型，非硬體精確（4.3" 觸控 LCD／雙 Gb Ethernet switch／WiFi/藍牙／TPS65910 電源管理 IC 均未模擬），不支援 U-Boot」。
- **明確排除在機型模型範圍外**：boot ROM、U-Boot/SPL、NAND/eMMC/SD 控制器、乙太網路（CPSW，含真實板子上的雙 Gb Ethernet switch）、I2C、SPI、PWM/eCAP/eQEP、ADC、RTC、看門狗、USB、4.3" 觸控 LCD 控制器、WiFi/藍牙、TPS65910 電源管理 IC、完整精確的 PRCM 狀態機。這些在「開到主線 Linux console」的最小開機範圍之外，一律不模擬（PRCM/Control Module 僅有前述簡化 stub）。

## 測試決策

- 單一驗證面：一個**黑盒 functional test**，比照 `tests/functional/arm/test_cubieboard.py` 這類既有簡單 ARM 機型測試的寫法（繼承 `LinuxKernelTest`，用 `Asset` 快取 kernel/initrd 下載，`self.vm.add_args('-kernel', ..., '-dtb', ..., '-initrd', ...)`，用 `self.wait_for_console_pattern(...)` 對 console 輸出做斷言）。
- 不打算為 `am335x_intc`/`am335x_timer` 寫獨立單元測試或 qtest 層級的裝置測試。只斷言外部可觀察的行為（kernel 開機成功、到達 console、可執行一個 shell 指令），跟這個專案既有測試（見 `raspi5-support`）「測行為、不測實作細節」的一貫偏好一致。
- 測試素材：一份 mainline/發行版的 `armhf` kernel（`zImage`），搭配本案自行撰寫、隨測試一起維護的精簡 `tmdssk3358.dts`（在測試 setUp 階段用 `dtc` 編譯），以及一份輕量 `armhf` initrd（可重用其他既有 ARM functional test 已在用的 rootfs 素材，如果 kernel config 相容）。
- 「完成」的驗收標準：functional test 能到達 Linux console 登入提示符或 shell，並成功跑完一次 `exec_command_and_wait_for_pattern`。

## 範圍之外

- Boot ROM、U-Boot/SPL，以及任何從 NAND/eMMC/SD 開機的路徑（見「你要的『跑 u-boot』實際上是指哪一種？」討論，維持原案，不支援）。
- 完整、硬體精確地重現 TMDSSK3358 這塊實體板子。這是它的最小開機子集模型，不是完整板子模型：只模擬開機到 console 必要的裝置。
- 乙太網路（CPSW，含真實板子上的雙 Gb Ethernet switch）、MMC/SD 控制器、I2C、SPI、PWM/eCAP/eQEP、ADC、RTC、看門狗、USB、4.3" 觸控 LCD 控制器、WiFi/藍牙、TPS65910 電源管理 IC。
- 完整精確的 PRCM 狀態機、時鐘樹、pinmux 邏輯，只有前述「回傳固定就緒位型」的簡化 stub。
- 完全硬體精確的 AM3358 模擬。這份規格的範圍是「能把主線 Linux 開機到 console」，不是「每一個暫存器都符合真實硬體行為」。
- 沿用上游 `am33xx.dtsi`／真實 BeagleBone 裝置樹，本案自行撰寫並維護精簡 dtb。

## 補充說明

- 截至本規格書撰寫當下（2026-08-13），在 `qemu-devel` 或 `gitlab.com/qemu-project` 上未確認是否已有 AM335x/AM3358 支援的既有 RFC 或 patch；提交上游前建議先搜尋一次，避免重工。
- 考慮過但**放棄**的較低工作量替代方案：既然裝置樹是自行撰寫、不必對應真實硬體，理論上可以直接用 QEMU 既有的通用 IP（例如 `hw/timer/sp804.c` + `arm,sp804`、`hw/intc/arm_gic.c` + `arm,gic-400`）拼出一張「Cortex-A8 板子」，完全不用寫 `am335x_intc.c`/`am335x_timer.c` 兩顆新裝置，工作量會小很多。但這樣就只是「CPU 型號恰好是 cortex-a8 的通用板子」，跟訪談中確認的「上游驅動＝`ti,am33xx-intc`、`dmtimer`」這個共識不符，所以維持寫 TI 專屬的 INTC/Timer 裝置這個方向；如果之後想換成這個更省力的路線，需要回頭跟需求方確認是否能接受犧牲「AM3358 專屬周邊」這個身份。
- PRCM/Control Module 用 `unimplemented-device` 蓋牌是否真的足夠讓 am33xx clock 驅動與 `pinctrl-single` 探測通過，目前只是根據 `IDLEST` 欄位語義做的推論，尚未實測；規劃階段標記的風險是，如果實測卡住，需要補一顆小型專用 stub 裝置（模式可參考 `hw/misc/imx25_ccm.c`），而不是回頭重新引入完整 PRCM 狀態機。
- DMTimer2（`0x48040000`）作為唯一計時器同時身兼 clocksource 與 clockevent，選它是因為它是真實 AM3358 上常見的角色分配之一；如果實測發現主線 `ti,am335x-timer` 驅動需要至少兩顆計時器才能正常運作（例如某些 kernel config 假設 clocksource 與 clockevent 分開），需要回頭補第二顆計時器實例，同一顆 `am335x_timer.c` 裝置程式碼應可重複實例化，不需要重寫。
