# 03: tmdssk3358 文件與 MAINTAINERS

**What to build:** 讓 `tmdssk3358` 機型像既有機型一樣可被發現、可被查詢負責人，並在文件中清楚標示這是 TMDSSK3358 的最小開機子集模型（非硬體精確、不支援 U-Boot）以及已實作/缺少的裝置清單。

**Blocked by:** 01（文件內容需要對照已定案的記憶體映射與裝置清單）

**Status:** ready-for-agent

- [ ] 新增 `docs/system/arm/tmdssk3358.rst`，格式比照 `docs/system/arm/cubieboard.rst`：列出 emulated devices（Timer、UART、INTC），並明確加註「這是 TMDSSK3358 的最小開機子集模型，非硬體精確（LCD／雙 Gb Ethernet switch／WiFi/藍牙／TPS65910 PMIC 均未模擬），不支援 U-Boot／boot ROM／SD 開機」
- [ ] `docs/system/arm/tmdssk3358.rst` 加進 `docs/system/target-arm.rst`（或現有機型索引）的目錄
- [ ] `MAINTAINERS` 新增 `AM3358` 條目，`F:` 涵蓋 `hw/arm/am3358.c`、`hw/arm/tmdssk3358.c`、`hw/intc/am335x_intc.c`、`hw/timer/am335x_timer.c`、對應 `include/hw/*` 標頭，狀態沿用既有 `OMAP` 條目的模式（`Odd Fixes`，因為目前沒有專屬維護者）
- [ ] 確認 `docs/system/arm/tmdssk3358.rst` 與 spec 的「範圍之外」章節一致，明確列出乙太網路／MMC-SD／I2C／SPI／PWM／ADC／RTC／看門狗／USB／LCD 均未實作
