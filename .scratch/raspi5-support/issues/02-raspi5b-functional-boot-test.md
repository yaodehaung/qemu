# 02 — raspi5b 的 functional boot test

**What to build:** 一個黑盒 functional test，驗證新機型能把 Linux 開到 console，並成功執行一個 shell 指令，讓核心開機路徑和周邊連線的回歸能被自動抓到。

**Blocked by:** 01 — BCM2712 與 raspi5b 的核心開機路徑

**Status:** ready-for-agent

- [ ] 測試可以穩定啟動新機型並觀察到預期的開機輸出
- [ ] 測試能確認系統已進入可操作狀態，並完成一次命令執行與回應比對
- [ ] 測試失敗時能清楚反映是開機、console 還是裝置連線問題