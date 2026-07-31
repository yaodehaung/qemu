# Q32：新增 QEMU linux-user ISA target 的介面與參考實作研究

## 研究問題與結論

本報告回答：為 QEMU 新增一個 32 位元、小端序、固定 32-bit 指令、僅支援 `linux-user` 的全新 ISA target（Q32），需要實作哪些 CPU、TCG/decodetree、ELF/ABI、linux-user、建置與測試介面，以及應以哪個既有 target 為參考。

結論是：**不要完整複製單一 target；以 OpenRISC（`or1k`）作為 `target/q32/` 的主要結構參考，以 RISC-V 32 的 syscall、程序進入與 signal ABI 作為 `linux-user/q32/` 的語意參考，僅以 Hexagon 證明「user-only、32-bit、小端 target」的建置形狀。**

- OpenRISC 是 32-bit、固定寬度、32 個 GPR 的傳統純量 RISC，使用標準 `decodetree` 和 `TranslatorOps`；其 target Meson 還清楚分離共用、user-only 與 system-only source set（[`target/or1k/meson.build:1-31`](../../target/or1k/meson.build#L1-L31)）。缺點是它是大端、具有 delay slot，且 CPU state 帶有 system-mode/浮點等 Q32 不需要的內容。
- RISC-V linux-user 的 `ecall` 正是以 `a7`（x17）傳 syscall number、`a0`–`a5`（x10–x15）傳六個參數、`a0` 收結果，並處理 restart/sigreturn 特殊返回；這與 Q32 已選 ABI 完全同形（[`linux-user/riscv/cpu_loop.c:48-70`](../../linux-user/riscv/cpu_loop.c#L48-L70)）。
- Hexagon 的設定檔證明省略 `TARGET_BIG_ENDIAN` 即採預設小端，且可只有 `hexagon-linux-user.mak`、沒有 softmmu 設定（[`configs/targets/hexagon-linux-user.mak:1-6`](../../configs/targets/hexagon-linux-user.mak#L1-L6)）。但其 target 依賴多階段語意與 decoder 產生流程（[`target/hexagon/meson.build:26-40`](../../target/hexagon/meson.build#L26-L40)、[`target/hexagon/meson.build:76-104`](../../target/hexagon/meson.build#L76-L104)），不應成為簡單 Q32 的程式骨架。

以下引用均指本研究時的 QEMU 工作樹；它們是本地 QEMU 原始碼或隨原始碼提供的官方開發文件。

## 一、`target/q32/` 必要介面

### 1. CPU 型別與架構狀態

至少需要：

- `cpu-qom.h`：QOM CPU 型別宣告與 cast macro。
- `cpu.h`：`CPUArchState`（Q32 最小可只有 `uint32_t gpr[32]`、`uint32_t pc`，以及執行例外所需狀態）、`ArchCPU`、例外編號、對外函式原型。
- `cpu-param.h`：`TARGET_PAGE_BITS` 與 32-bit virtual address width。OpenRISC 的對應最小宣告見 [`target/or1k/cpu-param.h:8-13`](../../target/or1k/cpu-param.h#L8-L13)，其 `CPUArchState`/`ArchCPU` 形狀見 [`target/or1k/cpu.h:225-288`](../../target/or1k/cpu.h#L225-L288)。
- `cpu.c`：QOM 註冊、CPU model、reset、`set_pc`/`get_pc`、state dump、TCG CPU operations。OpenRISC 的 PC hooks 見 [`target/or1k/cpu.c:29-42`](../../target/or1k/cpu.c#L29-L42)，必要的 TCG hooks 集合可由 [`target/or1k/cpu.c:254-273`](../../target/or1k/cpu.c#L254-L273) 取得；CPU class 安裝 PC 與 dump hooks 的位置在 [`target/or1k/cpu.c:275-290`](../../target/or1k/cpu.c#L275-L290)。

Q32 是 user-only，因此不需要 machine、interrupt controller、system MMU 或 migration VMState。應仿照 OpenRISC 將 system-only 實作隔離，而不是留下空殼：其 `meson.build` 把 `interrupt.c`、`machine.c`、`mmu.c` 放入 `target_system_arch`，共用翻譯器則放 `target_arch`（[`target/or1k/meson.build:3-31`](../../target/or1k/meson.build#L3-L31)）。Q32 可只註冊 `target_arch += {'q32': q32_ss}`，不建立 system source set。

### 2. TCG 全域、TB state 與精確例外恢復

`cpu.c`/`translate.c` 必須共同提供：

- 把 GPR、PC 映射為 TCG globals 的初始化函式；OpenRISC 以 `tcg_global_mem_new_i32()` 建立 PC 與暫存器映射（[`target/or1k/translate.c:94-135`](../../target/or1k/translate.c#L94-L135)）。Q32 的 `r0` 應在 translator 寫回路徑中丟棄，並保證讀值為零。
- `get_tb_cpu_state`：至少以 PC 定位 translation block；若 Q32 v1 沒有會影響解碼/執行語意的動態模式，TB flags 可為零。OpenRISC 範例見 [`target/or1k/cpu.c:44-53`](../../target/or1k/cpu.c#L44-L53)。
- `synchronize_from_tb` 與 `restore_state_to_opc`：讓 signal/例外能恢復到精確 guest PC。OpenRISC 的實作見 [`target/or1k/cpu.c:56-75`](../../target/or1k/cpu.c#L56-L75)；QEMU TCG 文件說明 host fault 時會用 host PC 查回 target PC（[`docs/devel/tcg.rst:151-168`](../../docs/devel/tcg.rst#L151-L168)）。
- `TCGCPUOps.initialize`、`translate_code`、`get_tb_cpu_state`、`synchronize_from_tb`、`restore_state_to_opc`、`mmu_index`。純 user-mode 不需要 system-only 的 `tlb_fill`、interrupt、reset execution hooks；OpenRISC 正以 `CONFIG_USER_ONLY` 分隔它們（[`target/or1k/cpu.c:254-272`](../../target/or1k/cpu.c#L254-L272)）。

### 3. decodetree 與 translator

建議建立：

- `insns.decode`：Q32 的 32-bit encoding patterns、共用 formats、fields 與 argument sets。
- `translate.c`：包含 Meson 產生的 `decode-insns.c.inc`、每條指令的 `trans_*` 函式、TCG load/store/ALU/branch 生成，以及 `TranslatorOps`。
- `meson.build`：`gen = decodetree.process('insns.decode')` 並把 `gen` 加入 Q32 source set；這是 OpenRISC 的直接模式（[`target/or1k/meson.build:1-14`](../../target/or1k/meson.build#L1-L14)）。

官方 decodetree 規格指出 pattern 由 fixed bits/mask 比對，fields 被抽出並交給 translator（[`docs/devel/decodetree.rst:7-21`](../../docs/devel/decodetree.rst#L7-L21)）；formats 可消除重複欄位配置（[`docs/devel/decodetree.rst:129-165`](../../docs/devel/decodetree.rst#L129-L165)）；匹配後呼叫對應的 translator function（[`docs/devel/decodetree.rst:176-207`](../../docs/devel/decodetree.rst#L176-L207)）。這正適合尚在固定 encoding 的 Q32，且 decoder 的 pattern overlap 檢查能提早暴露編碼衝突。

translator 最小流程是：

1. `init_disas_context` 設定 TB 狀態並限制 TB 不跨 guest page；OpenRISC 以固定 4-byte 指令計算 page bound（[`target/or1k/translate.c:1519-1534`](../../target/or1k/translate.c#L1519-L1534)）。
2. `insn_start` 記錄可恢復的 guest PC（[`target/or1k/translate.c:1550-1556`](../../target/or1k/translate.c#L1550-L1556)）。
3. `translate_insn` 以 target endian 讀 32 bits、呼叫 decoder、未匹配時產生 illegal-instruction exception，最後 PC 加 4（[`target/or1k/translate.c:1558-1567`](../../target/or1k/translate.c#L1558-L1567)）。Q32 應用 little-endian `MemOp`，不可照抄 OpenRISC 的 big-endian helper。
4. `tb_stop` 正確寫回 next PC，對直接與間接跳轉選用 TB chaining 或回 dispatcher；官方文件列出 `goto_tb + exit_tb` 所需步驟與不得跨 page 的限制（[`docs/devel/tcg.rst:68-125`](../../docs/devel/tcg.rst#L68-L125)）。
5. 用 `TranslatorOps` 呼叫 `translator_loop`；完整骨架見 [`target/or1k/translate.c:1636-1651`](../../target/or1k/translate.c#L1636-L1651)。

對 8/16/32-bit load/store，應讓 ISA 對齊決策映射到 TCG `MemOp`，同時指定 sign/zero extension 與 little endian。官方 load/store 文件明定 `MemOp` 包含 alignment，guest virtual access 可能拋出 alignment/MMU fault（[`docs/devel/loads-stores.rst:73-98`](../../docs/devel/loads-stores.rst#L73-L98)），並列出 `_le` 存取形式（[`docs/devel/loads-stores.rst:106-121`](../../docs/devel/loads-stores.rst#L106-L121)）。

### 4. 例外與 syscall 出口

Q32 至少需內部例外：syscall、illegal instruction、debug，以及是否另行表示 alignment fault。translator 遇 `ecall` 或非法編碼時設定 `CPUState.exception_index`，同步精確 PC，退出 TCG loop；linux-user 的 `cpu_loop` 再把它轉成 host syscall 或 guest signal。

這條邊界很重要：架構 translator 不直接呼叫 Linux syscall。RISC-V 的 user loop 將 `RISCV_EXCP_U_ECALL` 交給 `do_syscall()`，非法指令則送 `SIGILL`（[`linux-user/riscv/cpu_loop.c:48-76`](../../linux-user/riscv/cpu_loop.c#L48-L76)）；OpenRISC 也將 alignment/illegal/debug 分別映射為 `SIGBUS`/`SIGILL`/`SIGTRAP`（[`linux-user/or1k/cpu_loop.c:55-68`](../../linux-user/or1k/cpu_loop.c#L55-L68)）。

### 5. disassembler、GDB 與 helper 的範圍

- `disas.c` 不是執行正確性的核心，但 CPU state dump、`-d in_asm` 與除錯品質會依賴它。OpenRISC 重用同一份 generated decoder（[`target/or1k/disas.c:25-47`](../../target/or1k/disas.c#L25-L47)），Q32 可採同樣方式。
- 若提供 GDB remote register access，需 `gdbstub.c` 與 XML register description，並在 target config 設 `TARGET_XML_FILES`；OpenRISC 把 gdbstub 放在 user source set（[`target/or1k/meson.build:16-17`](../../target/or1k/meson.build#L16-L17)），config 宣告 XML（[`configs/targets/or1k-linux-user.mak:1-6`](../../configs/targets/or1k-linux-user.mak#L1-L6)）。最小 bring-up 可延後，但正式交接標準宜包含。
- 純 ALU、branch、load/store 優先直接產生 TCG ops；只有需要 C 語意、複雜 fault/狀態操作時才增加 `helper.h` 與 helper `.c`。不可把每條簡單指令都包成 helper，否則失去 TCG 最佳化與 chaining 效益。

## 二、`linux-user/q32/` 與 ABI 必要介面

### 1. `cpu_loop.c`：執行、syscall、signal 與初始狀態

Q32 的 `cpu_loop()` 應仿 RISC-V：

- 用 `cpu_exec_start()` / `cpu_exec()` / `cpu_exec_end()` / `qemu_process_cpu_events()` 驅動 guest（[`linux-user/riscv/cpu_loop.c:35-40`](../../linux-user/riscv/cpu_loop.c#L35-L40)）。
- `ecall` 後先把 PC 推進 4；以 `r17` 作 syscall number，`r10`–`r15` 作六參數；普通結果寫 `r10`。
- 對 `-QEMU_ERESTARTSYS` 把 PC 退回 4，讓 syscall 重啟；不要覆寫 `-QEMU_ESIGRETURN`/`-QEMU_ESETPC` 的架構狀態。RISC-V 的完整先例在 [`linux-user/riscv/cpu_loop.c:48-70`](../../linux-user/riscv/cpu_loop.c#L48-L70)。
- 每輪處理 pending signals（[`linux-user/riscv/cpu_loop.c:93-94`](../../linux-user/riscv/cpu_loop.c#L93-L94)）。
- `init_main_thread()` 設 `pc = ELF entry`、`r2 = initial stack`；同型的 RISC-V 作法見 [`linux-user/riscv/cpu_loop.c:97-103`](../../linux-user/riscv/cpu_loop.c#L97-L103)。

### 2. ELF 載入與 core register layout

需要 `target_elf.h` 定義：

- Q32 的 `ELF_MACHINE`（實驗階段必須先選一個明確、專案私用且不與現有值衝突的數值）與 `ELF_CLASS ELFCLASS32`。QEMU 共用 ELF loader 用 `ELF_MACHINE` 檢查 machine（[`linux-user/elfload.c:123-125`](../../linux-user/elfload.c#L123-L125)），並由 target endianness 自動選 `ELFDATA2LSB/MSB`（[`linux-user/elfload.c:108-112`](../../linux-user/elfload.c#L108-L112)）。
- `target_elf_gregset_t` 與 `elf_core_copy_regs()`（若啟用 `HAVE_ELF_CORE_DUMP`）。RISC-V 將 core GPR set 定義為 PC 加 x1–x31（[`linux-user/riscv/target_elf.h:21-28`](../../linux-user/riscv/target_elf.h#L21-L28)）；OpenRISC 在 `elfload.c` 實作暫存器複製與 target-endian swap（[`linux-user/or1k/elfload.c:14-20`](../../linux-user/or1k/elfload.c#L14-L20)）。
- 若 Q32 的 ELF `e_flags` 暫無 CPU variant，可讓 `get_elf_cpu_model()` 固定回傳唯一 CPU model，如 OpenRISC（[`linux-user/or1k/elfload.c:9-12`](../../linux-user/or1k/elfload.c#L9-L12)）。

**風險：**地圖把「向上游申請正式 ELF/Linux 架構識別碼」列為範圍外，所以測試 ELF 的 `e_machine` 只能是暫時私有協議。它必須在 Q32 規格、ELF 產生器、QEMU `include/elf.h`/`target_elf.h` 三處一致，且明確標註不可視為上游 ABI。QEMU 的既有 machine constants 集中在 [`include/elf.h:138-216`](../../include/elf.h#L138-L216)。

### 3. process ABI、clone/TLS 與 signal frame

`target_cpu.h` 至少提供：

- `cpu_clone_regs_child`：若給新 SP 就寫 `r2`，child return (`r10`) 設 0。
- `cpu_clone_regs_parent`。
- `cpu_set_tls`：這需要 Q32 ABI **先指定 thread pointer register**；目前地圖尚未指定，是阻擋完整 linux-user ABI 的缺口。
- `get_sp_from_cpustate` 回 `r2`。

RISC-V 的四個 hook 可直接作語意模板（[`linux-user/riscv/target_cpu.h:4-26`](../../linux-user/riscv/target_cpu.h#L4-L26)），但 register number 必須改成 Q32 決策。

`signal.c` 必須定義 Linux-visible `sigcontext/ucontext/rt_sigframe` layout，以及：

- 選 signal stack、16-byte 對齊；RISC-V 在 [`linux-user/riscv/signal.c:62-77`](../../linux-user/riscv/signal.c#L62-L77) 實作。
- `setup_rt_frame` 保存 PC/GPR/sigmask/altstack，設定 handler PC、SP、signal arguments 與 return address（[`linux-user/riscv/signal.c:80-138`](../../linux-user/riscv/signal.c#L80-L138)）。
- `do_rt_sigreturn` 還原完整狀態與 signal mask（[`linux-user/riscv/signal.c:148-203`](../../linux-user/riscv/signal.c#L148-L203)）。
- `setup_sigtramp` 產生「載入 `rt_sigreturn` syscall number + `ecall`」的兩條 Q32 指令；RISC-V 範例見 [`linux-user/riscv/signal.c:206-215`](../../linux-user/riscv/signal.c#L206-L215)。因此 signal 支援會反過來約束 Q32 立即數載入能力與 ABI。

### 4. syscall table 與 target ABI headers

Q32 採 Linux generic syscall numbering，仍需要：

- `syscall.tbl` 與 `syscallhdr.sh`，以及 `linux-user/q32/meson.build` 中的 syscall header generator。OpenRISC generator 形狀見 [`linux-user/or1k/meson.build:1-5`](../../linux-user/or1k/meson.build#L1-L5)，target config 用 `TARGET_SYSTBL`/`TARGET_SYSTBL_ABI` 選表與 ABI tags（[`configs/targets/or1k-linux-user.mak:1-6`](../../configs/targets/or1k-linux-user.mak#L1-L6)）。
- 一組 target ABI headers：`target_syscall.h`、`target_signal.h`、`target_errno_defs.h`、`target_fcntl.h`、`target_mman.h`、`target_resource.h`、`target_structs.h`、`target_ptrace.h`、`target_proc.h`、`target_prctl.h`、`sockbits.h`、`termbits.h`。可先從採 generic 32-bit ABI 的 RISC-V/OpenRISC 比對哪些可薄封裝共用定義，不能盲目複製 kernel ABI-specific struct/layout。

這些不是只有編譯用樣板：`linux-user/qemu.h` 會直接 include `target_syscall.h`（[`linux-user/qemu.h:4-13`](../../linux-user/qemu.h#L4-L13)），而 signal、ioctl、fcntl、termios、ptrace 等資料結構是 guest/host 轉換契約。

## 三、建置整合清單

應加入：

1. `configs/targets/q32-linux-user.mak`：
   - `TARGET_ARCH=q32`
   - `TARGET_LONG_BITS=32`
   - 不設定 `TARGET_BIG_ENDIAN`（Meson 缺省為 `n`，見 [`meson.build:3334-3338`](../../meson.build#L3334-L3338)）
   - `TARGET_NOT_USING_LEGACY_NATIVE_ENDIAN_API=y`
   - `TARGET_SYSTBL=syscall.tbl` 與已決定的 generic 32-bit ABI tags
   - 若實作 GDB，設定 `TARGET_XML_FILES`
2. `target/meson.build` 加 `subdir('q32')`；既有 target 架構清單在 [`target/meson.build:1-19`](../../target/meson.build#L1-L19)。
3. `target/q32/meson.build` 產生 decoder、建立 `q32_ss` 並註冊 `target_arch`。
4. `linux-user/meson.build` 加 `subdir('q32')`；既有 ABI subdirs 在 [`linux-user/meson.build:42-60`](../../linux-user/meson.build#L42-L60)。
5. `linux-user/q32/meson.build` 註冊 syscall header generator。
6. `include/elf.h` 增加暫定 `EM_Q32`，並讓 `target_elf.h` 使用它。
7. 若提供 disassembler/GDB XML，加入對應 source 與資料檔；若沒有，應在 v1 handoff 明確列為暫緩，而非默認已支援。

建置驗證至少包含單獨 configure `--target-list=q32-linux-user`，確保沒有 system-only 依賴；再與現有 user targets 一起建置，檢查 target-specific header 或符號污染。

## 四、測試策略與最低驗收門檻

### 1. decoder 與逐指令測試

- 對每條合法 encoding：執行語意、PC 前進、`r0` 不可寫。
- 對保留/未配置 encoding：必須穩定送 `SIGILL`，不能落入另一 pattern。decodetree 的 fixed-bit 規則與 overlap group 行為在 [`docs/devel/decodetree.rst:7-18`](../../docs/devel/decodetree.rst#L7-L18)、[`docs/devel/decodetree.rst:209-232`](../../docs/devel/decodetree.rst#L209-L232)。
- ALU 邊界：0、全 1、signed min/max、移位 0/31/超寬（依 Q32 規格決策）。
- branch/jump：taken/not-taken、正負 PC-relative displacement、direct/indirect、link address。
- memory：8/16/32-bit、signed/unsigned extension、小端 byte layout、跨頁與 misalignment（依最終規格決定成功或 `SIGBUS`）。

### 2. linux-user/ABI 測試

用範圍內的小型 ELF 產生器建立不依賴 GCC/binutils 的靜態測試 ELF，至少驗證：

- ELF32 + little endian + Q32 `e_machine` 接受；錯 class/endian/machine 拒絕。共用 loader 同時檢查 class/data（[`linux-user/elfload.c:288-289`](../../linux-user/elfload.c#L288-L289)）。
- entry PC、初始 SP 16-byte alignment、`argc/argv/envp/auxv` 可由 guest 讀取；`image_info` 保存 entry、stack、argc/argv/envp 等 loader 結果（[`linux-user/qemu.h:27-49`](../../linux-user/qemu.h#L27-L49)）。
- `write`/`exit` 等基本 syscall；參數暫存器、negative errno、未知 syscall。
- 可重啟 syscall 的 PC 回退；`clone` child return、new SP；TLS（需先決定 TP register）。
- illegal instruction、alignment、guest memory fault、debug trap 對應的 guest signal。
- signal handler 三參數、alternate stack、nested signal、`rt_sigreturn` 完整還原 PC/GPR/mask。

### 3. 測試接入方式

建立 `tests/tcg/q32/`。Q32 尚無交叉編譯器，因此測試規則應先調用 Q32 ELF/指令產生器，再以 `qemu-q32` 執行；不要假裝沿用一般 C cross-compile 流程。現有 OpenRISC 指令測試放在 [`tests/tcg/or1k/`](../../tests/tcg/or1k/)，Hexagon 則以 target-specific `Makefile.target`、startup assembly 與 C/assembly 測試組合（例如 [`tests/tcg/hexagon/Makefile.target`](../../tests/tcg/hexagon/Makefile.target)、[`tests/tcg/hexagon/crt.S`](../../tests/tcg/hexagon/crt.S)）。對 Q32，產生器輸入與 expected stdout/exit/signal 必須可重現並納入版本控制。

建議分三層驗收：

1. **build smoke**：`qemu-q32` 可建置、`--version` 可執行。
2. **raw instruction/ELF smoke**：一個最小 ELF 能執行 `write` 後 `exit(0)`。
3. **semantic matrix**：所有 v1 指令、非法 encoding、memory/branch 邊界、syscall restart、signal round-trip 與 ELF rejection cases。

## 五、建議的實作邊界與順序

依目前證據，合理工作包順序是：

1. **先鎖定 ISA/ABI 未決項**：完整 encoding、alignment、PC/exception 精確語意、暫定 ELF machine value、thread-pointer register、signal context layout、signal trampoline 可表達性。這些若未決，target 介面雖可建空殼但不能正確完成。
2. **最小 `target/q32`**：CPU state/QOM、TCG hooks、decodetree、ALU/branch/load-store、`ecall`/illegal exception；只建立 user-mode sources。
3. **最小 `linux-user/q32`**：ELF loading、initial PC/SP、generic syscall bridge、clone/TLS hooks、signal ABI。
4. **測試產物工具與 TCG tests**：先 raw ELF smoke，再 semantic matrix。
5. **可觀測性**：disassembler、state dump、GDB registers；在除錯完整測試矩陣前完成。

## 六、會影響 Wayfinder 後續決策的新發現

- 「完成 Q32 ELF 與行程 ABI」必須新增明確決策：**暫定 `e_machine` 值、thread-pointer register、signal context/ucontext layout、signal handler arguments、signal trampoline encoding**。僅有呼叫慣例與 syscall registers 不足以實作 linux-user。
- 「定義記憶體語意」必須把 alignment 規則精確映射到 TCG `MemOp`，並決定 misalignment 是由 TCG 完成還是轉 `SIGBUS`。
- 「選擇 QEMU target 模組邊界」應明確採 **OpenRISC target 骨架 + RISC-V linux-user ABI 路徑**，並排除 OpenRISC 的 delay slot、big-endian、system MMU/interrupt、浮點與架構狀態。
- 「測試產物工具」不能只產 instruction bytes；它至少需產生一致的 ELF header/program headers、初始可執行 segment、必要資料與 expected outcome，並能刻意產生非法 ELF/encoding 作負面測試。

## 最終建議

**主要參考 target：OpenRISC (`target/or1k`)。**它提供 Q32 最需要的簡潔 decodetree、固定 4-byte fetch、32-bit TCG globals、`TranslatorOps` 與 user/system source 分層。複製時只取結構，不取其 big-endian、delay-slot、flag/浮點/MMU/interrupt 語意。

**主要 linux-user 參考：RISC-V (`linux-user/riscv`)。**它的 x17/x10–x15/x10 syscall ABI、PC += 4、restart/sigreturn 與 Q32 決策直接對應；signal frame 與 clone/TLS hooks也展示尚待 Q32 規格補齊的 ABI 契約。

**Hexagon 只作邊界佐證，不作骨架。**它證明 32-bit little-endian user-only target 可存在，但 VLIW packet 與生成器複雜度遠超 Q32 所需。
