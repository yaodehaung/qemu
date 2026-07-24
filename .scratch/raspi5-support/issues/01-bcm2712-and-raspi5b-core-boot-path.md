# 01 — BCM2712 與 raspi5b 的核心開機路徑

**What to build:** 讓使用者可以選到一個新的 Raspberry Pi 5 機型，並用外部裝置樹與可選 initrd 把 Linux kernel 開到序列 console；底層要有對應的 SoC、周邊與中斷連線，且只提供最小可開機所需的硬體集合。

**Blocked by:** None — can start immediately

**Status:** ready-for-agent

- [ ] 新機型能被建置系統編進去並在命令列中選用
- [ ] 開機流程能成功載入 kernel、DTB，並在 console 上到達可互動的 Linux 啟動狀態
- [ ] 最小硬體集合只包含可支撐 console 開機所需的裝置，其餘缺少裝置能在 Linux 端優雅地延遲或跳過探測