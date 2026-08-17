# 02 — am3352-evm functional boot test

**What to build:** 一個自動化的黑盒 functional test，把主線／發行版 armhf Linux kernel 用 `am3352-evm` 開機到 console 提示符並執行一個 shell 指令，讓 `am3352-evm` 模型日後出現迴歸時能被 CI 自動抓到。

**Blocked by:** 01（需要能建置、能實際開機的 `am3352-evm` 機型與精簡裝置樹）

**Status:** ready-for-agent

- [ ] 新增 `tests/functional/arm/test_am3352_evm.py`，比照 `tests/functional/arm/test_cubieboard.py` 這類既有簡單 ARM 機型測試的寫法：繼承 `LinuxKernelTest`，用 `Asset` 快取 kernel／initrd 下載
- [ ] 測試以 `self.vm.add_args('-kernel', ..., '-dtb', ..., '-initrd', ...)` 帶入 01 產出的精簡裝置樹與對應 kernel/initrd
- [ ] 用 `self.wait_for_console_pattern(...)` 斷言開機訊息與登入提示符／console 出現
- [ ] 用 `exec_command_and_wait_for_pattern(...)` 執行至少一個 shell 指令並驗證輸出，確認不只是「看起來卡在提示符」而是真的有可互動的 shell
- [ ] 測試在 `meson.build` 的 functional test 清單中正確註冊，`meson test` 能跑過
- [ ] 不新增 `am335x_intc`/`am335x_timer` 的獨立單元測試或 qtest；只斷言外部可觀察行為（依 spec「測試決策」）
