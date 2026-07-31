# 設定 Q32 驗證與實作交接標準

Type: grilling
Status: resolved
Blocked by: 05, 06

## 問題

最終設計必須定義哪些一致性案例、QEMU 測試層級、失敗診斷、文件及工作套件邊界，才能在沒有未決架構問題的情況下開始實作？

## 答案

### Q32 v1 完成 gate

必做：

- `q32-linux-user` 可單獨 configure/build，並實作全部 v1 指令、保留編碼與精確 PC/fault。
- 靜態 ELF32、初始 stack/auxv、generic syscall、errno/restart、rt signal、process-style fork/clone 與 thread clone 拒絕。
- 自修改程式碼與 TB invalidation、完整 disassembler/state dump、deterministic 測試產物工具與 TCG 接入。

非 gate：GDB stub、system-mode、dynamic linker/shared library、正式 compiler toolchain、效能最佳化與 fuzzing。

### 五層驗證

1. **Build**：單獨 Q32 與多 user-target build，且無 system-mode 依賴。
2. **ELF**：正常 load/write/exit，以及 class/endian/machine/flags/entry/segment 接受與拒絕。
3. **ISA**：每條合法指令、各類保留編碼、r0、PC、立即數、branch/jump 邊界。
4. **Memory/fault**：對齊、小端、擴展、page boundary、精確 SIGBUS/SIGSEGV、無部分 store、自修改程式碼。
5. **Process ABI**：initial stack/auxv、syscall/errno/restart、signal/altstack/nesting/sigreturn、fork/clone 與 thread 拒絕。

任何一層失敗都不能宣稱 v1 完成。

### 每個案例的 oracle

- 指令 bytes/expected words、ELF/program header 關鍵欄位。
- 初始 register/memory，或明確引用標準 process entry。
- stdout、exit status 或 signal；signal 另驗證 `si_code/si_addr`。
- 需要時驗證最終 PC、GPR 與指定 memory。
- 負面案例記錄顯式 mutation；名稱描述行為，不能只用編號。
- 不接受只驗證「沒有 crash」。

### ISA 最低矩陣

- 算術值：0、1、全一、signed min/max；加減環繞。
- shift：0、1、31、32、33、全一。
- signed/unsigned 比較與 I-immediate -2048、-1、0、1、2047。
- B/J 最小、最大、前後、零位移；branch taken/not-taken。
- JAL/JALR link、rd=r0、環繞；JALR 四種低兩位組合均清零。
- LUI/AUIPC 零、最大 imm20 與 PC 環繞。
- 每種目的 register 指令至少一次寫 r0。
- ECALL、非法 opcode/subcode 與未對齊取指。

### Memory 最低矩陣

- LB/LBU/LH/LHU/LW 的零、符號位、全一；SB/SH/SW 截斷與 little-endian bytes。
- 每種寬度的合法邊界；16-bit odd address 與 32-bit mod 4=1/2/3。
- 位址加法在 `0xffffffff` 附近環繞。
- mapped/unmapped、read-only write、non-executable fetch。
- faulting load 不改 rd、faulting store 不改 bytes、load-to-r0 仍 fault。
- page boundary、自修改已翻譯指令。
- 每個 fault 驗證 signal、code、address 與 faulting PC。

### ELF/process 最低矩陣

- 單 RX 與 RX+RW、BSS zero fill。
- 錯 class/endian/machine/flags/type、entry alignment/execute permission。
- filesz>memsz、offset/vaddr congruence、address wrap、segment overlap。
- argc 0/1/many、env 空/多個、必要 auxv、所有 16-byte alignment。
- write/exit、unknown syscall、success/errno、restart/sigreturn 狀態。
- SIGILL/SIGBUS/SIGSEGV 三參數 handler、altstack、nested signal、完整 sigreturn、壞 frame。
- process clone/fork parent/child/new stack；thread flags 與 SETTLS 回 `-EINVAL`。
- core GPR set 的 PC 與 r1–r31。

### 必跑命令與證據

    mkdir build-q32
    cd build-q32
    ../configure --target-list=q32-linux-user
    make -j"$(nproc)"
    make check-tcg
    make check

另做至少包含 Q32、RISC-V 32、OpenRISC linux-user 的 multi-target build。記錄命令、exit status、host/compiler、skip 與既有非 Q32 failure 的基線；不得把 Q32 failure 標為 xfail。

### TDD 工作套件

1. **測試產物工具**：DSL/encoder/ELF/runner 與 golden。
2. **CPU/decoder 骨架**：QOM/state/Meson/decodetree/disassembler，先 build 與合法/非法 decode。
3. **TCG ISA 語意**：逐指令 red-green，完成 ALU/control/memory/fault。
4. **ELF/syscall bring-up**：loader/stack/ECALL/errno/restart，達成 write+exit smoke。
5. **Signal/process ABI**：frame/trampoline/sigreturn/fault/fork/clone。
6. **整合交接**：diagnostics、完整矩陣、multi-target、make check、文件、review。

每包先過 targeted tests；後一包不得修改既定 ISA/ABI 來繞過失敗。

### Code review 與交接

- 所有七張票 resolved，實作沒有未記錄規格偏差；每個指令、ABI 結構與 fault 可追溯至規格及測試。
- 無 Q32 compiler warning、assert、skip、xfail 或 flaky test；無 system-mode、RISC-V-specific、OpenRISC big-endian/delay-slot 邏輯滲入。
- ownership 符合 target/linux-user 邊界，產生器無手工 blob 唯一 oracle。
- review 同時檢查 Standards（QEMU style/QOM/TCG/Meson/error handling）與 Spec（七張票與矩陣）。
- findings 均修正，或明確記錄為不影響 v1 gate 的後續；提交保持六個工作套件的可審查邊界並附測試證據。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#設定-q32-驗證與實作交接標準)。
