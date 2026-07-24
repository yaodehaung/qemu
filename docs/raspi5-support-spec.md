# 規格書：QEMU 上的 Raspberry Pi 5（BCM2712）最小開機支援

狀態：ready-for-agent
負責人：nickhuang（第一次貢獻 QEMU hw/arm 的 patch）
目標：QEMU 上游主線（`qemu-devel`，hw/arm maintainer：Peter Maydell；reviewer：Philippe Mathieu-Daudé）

## 問題陳述

QEMU 目前已經模擬 `raspi0`/`raspi1ap`/`raspi2b`/`raspi3ap`/`raspi3b`/`raspi4b` 這些機型（見 `docs/system/arm/raspi.rst`），涵蓋從 BCM2835 到 BCM2711。但目前沒有對應 Raspberry Pi 5 的機型——它用的是 Broadcom 的 BCM2712 SoC。想要在 Raspberry Pi 5 的 Linux image（kernel、bootloader、CI pipeline）上開機、測試或開發的人，現在完全沒辦法在 QEMU 底下做這件事，只能用真實硬體。

## 解決方案

新增一個 QEMU 機型（`raspi5b`），模擬能讓主線 Linux kernel 開機到 console 登入提示符所需的最小 BCM2712 硬體集合，沿用 `raspi3b`/`raspi4b` 已經在用的 `-kernel`/`-dtb`/`-initrd` 工作流程。範圍刻意對齊 Andrea Porta（SUSE）當初送進主線 Linux 的「最小開機」支援：CPU、RAM、一顆 PL011 UART、一顆 GIC-400、一顆 BCM2835 相容的系統計時器，以及一顆 SDHCI 控制器。RP1（真實硬體上透過 PCIe 掛載、提供 GPIO/USB/Ethernet 等功能的 I/O 南橋晶片）以及 PCIe 本身明確排除在範圍外——真實 BCM2712 硬體上有一顆獨立於 RP1 之外、直接掛在 SoC 上的 debug UART，所以「能開出 console」的最小開機並不需要 PCIe/RP1 模擬。

## 使用者故事

1. 身為 QEMU 貢獻者，我想要有一個 `raspi5b` 機型，這樣我就能在沒有實體硬體的情況下，用模擬環境開機一份 Raspberry Pi 5 的 Linux kernel image。
2. 身為開發 BCM2712 支援的 kernel 開發者，我想要能對著 QEMU 的 SoC 模型測試 kernel 改動，這樣我就能比燒 SD 卡到真實硬體更快地反覆迭代。
3. 身為 CI 系統維護者，我想要有一個自動化的 functional test，能把 Raspberry Pi 5 Linux 開機到 console 提示符，這樣機型模型或 kernel 的 BCM2712 支援出現迴歸時能自動被抓到。
4. 身為 `-M raspi5b` 的使用者，我想要能傳入 `-kernel`、`-dtb`，以及可選的 `-initrd`，這樣開機流程就能跟 `raspi3b`/`raspi4b` 一致，不用學新的命令列慣例。
5. 身為使用者，我想要模擬的 CPU 是 `cortex-a76`（4 核心），這樣機型才能對應真實 Raspberry Pi 5 的應用處理器。
6. 身為使用者，我想要有一個可運作的序列 console（PL011 UART，位址對應真實 BCM2712 的 debug UART 位址），這樣我才能看到 kernel 開機輸出並操作 shell。
7. 身為使用者，我想要中斷能正確透過模擬的 GIC-400 路由，這樣 UART、計時器、SD 控制器才能像真實硬體一樣向 CPU 發訊號。
8. 身為使用者，我想要有一個 SD/MMC 開機裝置（SDHCI 相容控制器，位址對應真實 BCM2712 位址），這樣我才能像真實 Raspberry Pi 5 從 SD 卡開機一樣開機 kernel/rootfs。
9. 身為在沒有 RP1/PCIe 驅動可用情況下開機的使用者，我想要缺少的周邊裝置能優雅地失敗（驅動探測跳過/延遲），而不是卡住開機流程，這樣「最小」硬體集合仍然足夠開到 console。
10. 身為 qemu-devel 上的 reviewer，我想要新程式碼的組織方式跟 Pi 4 用的 `bcm2838.c`/`bcm2838_peripherals.c`/`raspi4b.c` 一樣，這樣審查 Pi 5 這組 patch 時，只是跟一個已知良好的先例做直接對照，而不是面對一個全新的結構。
11. 身為 reviewer，我想要 `MAINTAINERS`、`docs/system/arm/raspi.rst`、以及 Kconfig/meson.build 的條目都跟著程式碼一起更新，這樣新機型才能像既有的 raspi 機型一樣被找到、被建置。
12. 身為未來的貢獻者，我想要這組 patch 一併附上 functional test（`tests/functional/aarch64/test_raspi5.py`），這樣新機型合併之後才不會悄悄地產生迴歸。
13. 身為閱讀 `docs/system/arm/raspi.rst` 的使用者，我想要 Raspberry Pi 5 機型「已實作/缺少」的裝置清單被記錄下來，這樣我一開始就知道 RP1/PCIe/USB/Ethernet/GPIO 沒有被模擬。

## 實作決策

- **新機型**：`raspi5b`，仿照 `raspi4b` 建模。
- **新增原始碼檔案**，仿照 `bcm2838` 的模式：
  - `hw/arm/bcm2712.c` —— SoC 容器物件（對應 `hw/arm/bcm2838.c`），把 CPU cluster、GIC、周邊物件接進位址空間，連接 IRQ 線路。
  - `hw/arm/bcm2712_peripherals.c` —— 周邊容器物件（對應 `hw/arm/bcm2838_peripherals.c`），實例化並映射：PL011 UART、BCM2835 相容系統計時器、SDHCI 控制器。
  - `hw/arm/raspi5b.c` —— 機型定義（對應 `hw/arm/raspi4b.c`），註冊 `raspi5b` 這個 `MachineClass`，設定預設 RAM 大小與 CPU 型號，在可重用之處呼叫 `hw/arm/raspi.c` 裡 `raspi_init` 那類共用開機膠水程式碼。
- **CPU**：`cortex-a76`，4 核心。已經在 QEMU TCG 裡實作完成（`target/arm/tcg/cpu64.c`），不需要新的 CPU 模型工作。
- **記憶體映射表**（來自上游主線 Linux 的 BCM2712 最小開機裝置樹，作為位址的依據——不是自己編的）：
  - PL011 UART（debug console）：`0x7d001000`，大小 `0x200`。
  - GIC-400：distributor/CPU/virt 介面分別在 `0x7fff9000`、`0x7fffa000`、`0x7fffc000`、`0x7fffe000`。
  - BCM2835 相容系統計時器：`0x7c003000`。
  - SDHCI 控制器（SD 卡開機用）：`0xfff000`。
  - RAM：從 `0x0` 開始、由 bootloader 決定大小的區域；確切預設大小待對照測試用的同一份參考裝置樹確認（上游 patch 裡是 `memory@0`、`0x28000000`／640 MiB，待裁簡版裝置樹定案後再確認）。
- **開機機制**：跟 `raspi3b`/`raspi4b` 一樣——QEMU 內部不合成裝置樹。`arm_load_kernel()`（`hw/arm/boot.c`）的 `dtb_filename` 直接來自 `-dtb` 命令列參數；機型程式碼**不會**設定 `get_dtb` callback。使用者必須自備一份位址跟上述記憶體映射表對得上的裝置樹 blob。
- **中斷路由**：GIC-400 的接法比照 `bcm2838.c` 現在把 UART0/系統計時器/SDHCI 的 IRQ 接進 GIC 的方式——重用這套接法，不另外發明新的。
- **Kconfig / build 接線**：擴充 `hw/arm/Kconfig` 裡的 `config RASPI`（已經 select 了 `PL011`、`SDHCI`；需確認 `ARM_GIC` 是否需要明確 select，或已經透過其他路徑被間接引入），並把三個新檔案加進 `hw/arm/meson.build`，比照既有的 `bcm2838.c`/`bcm2838_peripherals.c` 條目。
- **MAINTAINERS**：現有 `Raspberry Pi` 條目的檔案 glob `F: hw/*/bcm283*` **不會**匹配到新的 `bcm2712*` 檔名——需要明確新增一行（例如 `F: hw/*/bcm2712*`），與已經能匹配 `raspi5b.c` 的 `F: hw/arm/raspi*.c` 並列。
- **文件**：更新 `docs/system/arm/raspi.rst`，在機型表格裡列出 `raspi5b`（Cortex-A76、4 核心、RAM 大小待定），並明確新增「Implemented devices」／「Missing devices」條目——Missing 裡必須明確列出 RP1、PCIe Root Port、GPIO 控制器、USB、Ethernet，就跟現在 `raspi4b` 的 PCIe/GENET 被列為缺少一樣。
- **明確排除在機型模型範圍外**：RP1 南橋晶片、PCIe root complex/控制器、USB、Ethernet（GENET）、GPIO 控制器、I2C/SPI、VideoCore/GPU、溫度感測器、RNG——這些在沒有 RP1/PCIe 的情況下都無法從 BCM2712 觸及，所以這一階段都不模擬。

## 測試決策

- 單一驗證面，已與開發者確認：一個**黑盒 functional test**，`tests/functional/aarch64/test_raspi5.py`，比照 `tests/functional/aarch64/test_raspi4.py` 既有的寫法（繼承 `LinuxKernelTest`，用 `Asset` 快取 kernel/dtb/initrd 下載，`self.vm.add_args('-kernel', ..., '-dtb', ..., '-initrd', ...)`，用 `self.wait_for_console_pattern(...)` 和 `exec_command_and_wait_for_pattern(...)` 對 console 輸出做斷言）。
- 不打算為 `bcm2712_peripherals` 寫單元測試或 `qtest` 層級的裝置測試。只斷言外部可觀察的行為（kernel 開機成功、到達 console、能執行一個 shell 指令）——這跟現在 `raspi3`/`raspi4` 的測試方式一致，也符合這個專案「測行為、不測實作細節」的一貫偏好。
- 測試素材：kernel 加上一份從 LKML「Raspberry Pi 5 minimal boot support」那組 patch 衍生出來的裁簡版裝置樹（選它而不用官方 `raspberrypi/linux` 完整版 `bcm2712-rpi-5-b.dts`，因為後者包含這裡不模擬的 RP1/PCIe 節點）。用來支撐登入提示符斷言的 rootfs/initrd，會採用相容 `arm64` 的素材（如果 kernel config 相容，重用 `test_raspi4.py` 已經在用的 `groeck/linux-build-test` 這份既有素材）。
- 「完成」的驗收標準：functional test 能到達 Linux console 登入提示符（或至少像 `test_raspi4.py` 的 `test_arm_raspi4_initrd` 一樣，看到 `Boot successful.` 並成功跑完一次 `exec_command_and_wait_for_pattern`）。

## 範圍之外

- RP1 南橋模擬（GPIO、USB、Ethernet，以及真實硬體上掛在 RP1 之後的其他 UART/I2C/SPI/PWM）。
- PCIe root complex/控制器模擬（BCM2712 上確實有，且是存取 RP1 所需，但最小 console 開機不需要）。
- VideoCore firmware/property mailbox、framebuffer、溫度感測器、RNG——這些在既有的 `raspiN` 機型上都有，但不屬於這次針對 BCM2712 的最小化範圍。
- Compute Module 5（CM5）或任何其他基於 BCM2712、但不是 Raspberry Pi 5 Model B 的機型。
- 完全硬體精確的 BCM2712 模擬。這份規格的範圍是「能把主線 Linux 開機到 console」，不是「每一個暫存器都符合真實硬體行為」。
- 在 QEMU 內部合成/維護裝置樹——跟所有既有的 raspi 機型一樣，由使用者自備 `-dtb`。

## 補充說明

- 這是開發者第一次貢獻 QEMU 上游。預期會在 qemu-devel 上經過多輪審查；可對照的 `raspi4b`/BCM2838 那組 patch（2019 年，Philippe Mathieu-Daudé）合併前大約經過了 14 版修訂——這是用來校準審查週期長度的參考值，不是要剛好命中的目標。
- 截至本規格書撰寫當下（2026-07-23），在 qemu-devel 或 gitlab.com/qemu-project 上都沒找到任何 `bcm2712`/`raspi5` 支援的既有 RFC 或 patch——這是全新的工作，不是延續某個中斷的系列。
- 規劃階段標記出的一個風險：如果裁簡版測試裝置樹的位址，跟 `bcm2712_peripherals.c` 實際實作出來的位址出現落差，應該修正 QEMU 端的記憶體映射去對齊裝置樹——而不是反過來——因為這裡的裝置樹才是依據上游 Linux 最小開機 patch 而來的事實來源，不是 QEMU 端自己編出來的版面。
- 確切要用哪個 SD 控制器模型（`bcm2835_sdhost` 還是通用的 `sdhci-bus` 實例）應該對照真實 BCM2712 boot ROM／主線 Linux 驅動在 `0xfff000` 這個位址預期看到的控制器型號來選，而不是單純挑一個現有 QEMU 模型裡最好接的。
