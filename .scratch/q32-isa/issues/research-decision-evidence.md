# Q32 七項設計決策的第一手證據

本報告逐一支援 Q32 Wayfinder 的七張決策票。引用來源限於本工作樹中的 QEMU 原始碼／官方開發文件，以及 RISC-V、System V、Linux 的來源擁有者文件。文中的「來源事實」是來源直接規定或實作的內容；「推論／建議」則是把該先例套用到 Q32，並非來源替 Q32 作出的規定。

官方規格入口：RISC-V International 的 [非特權 ISA 規格](https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html)、[ELF psABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/)、System V ABI 的 [ELF gABI](https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html)，以及 Linux kernel 的 [generic syscall 表](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/scripts/syscall.tbl)。

## 確定 Q32 v1 指令編碼

### 已知約束

- Q32 已決定固定 32 位元指令、32 個 GPR，因此每個暫存器欄位至少 5 位元；`r0`、`PC` 的角色已在地圖確定。
- QEMU decodetree 的 pattern 以 fixed bits/mask 配對，fields 抽取後傳給 translator；formats 可共用欄位配置（來源事實：[`docs/devel/decodetree.rst:7-21`](../../../docs/devel/decodetree.rst#L7-L21)、[`docs/devel/decodetree.rst:129-165`](../../../docs/devel/decodetree.rst#L129-L165)）。因此合法與保留編碼必須能由互不含糊的 mask 表達。

### 可採先例

- RV32I 使用 7-bit major opcode，常見 R/I/S/B/U/J 六種格式；`rs1`、`rs2`、`rd` 固定在位元 15、20、7 起的 5-bit 欄位，I/S/B/J/U 立即數則由格式抽取（來源事實：[`target/riscv/insn32.decode:20-70`](../../../target/riscv/insn32.decode#L20-L70)）。這個固定欄位位置可降低 decoder 與硬體解碼複雜度。
- RISC-V 把 `ecall`、`ebreak` 定成完全固定的 32-bit pattern，而非只辨識 opcode（來源事實：[`target/riscv/insn32.decode:114-121`](../../../target/riscv/insn32.decode#L114-L121)）。
- RISC-V 規格明確保留 custom opcode 空間，並區分 reserved 與 custom；但該分類屬 RISC-V 自己的相容契約，Q32 只能借用方法，不能宣稱相容（來源事實：[RISC-V 非特權規格，Instruction-Length Encoding](https://docs.riscv.org/reference/isa/unpriv/rv-32-64g.html#base-instruction-length-encoding)）。

### 仍需人類決定

- major opcode 位數與各格式的精確 bit layout；branch/jump displacement 的位數、縮放與拼接次序。
- 未來擴充預留多少完整 opcode，以及「未配置」「保留」「永久非法」是否需要不同分類。
- NOP 是否有 canonical encoding；`ecall` 與非法模式是否配置完整固定 word。

### 建議答案

**推論／建議：**採 RV32I 式的 7-bit opcode 與固定 `rd[11:7]`、`rs1[19:15]`、`rs2[24:20]`，只保留 Q32 需要的 R/I/S/B/U/J 子集；branch/jump 立即數最低位隱含 0 或 00 必須明寫。至少預留四個完整 major opcode 作未來用途，所有未匹配 pattern 一律為非法指令。先以 decodetree 編譯時的 overlap 檢查驗證無歧義，再凍結編碼表。

## 定義 Q32 整數與控制流程語意

### 已知約束

- Q32 v1 是 32-bit 整數 ISA；固定 4-byte 指令使正常 sequential PC 為 `PC + 4`。
- QEMU linux-user 的 RISC-V loop 在 `ecall` 後把 PC 加 4；非法指令送 `SIGILL`，breakpoint/debug 送 `SIGTRAP`（來源事實：[`linux-user/riscv/cpu_loop.c:48-81`](../../../linux-user/riscv/cpu_loop.c#L48-L81)）。Q32 必須定義足夠精確的 exception PC，才能做相同映射。

### 可採先例

- RV32I 整數加減溢位採 modulo \(2^{32}\)，不拋 arithmetic exception；比較另分 signed/unsigned（來源事實：[RISC-V 非特權規格，RV32I Integer Computational Instructions](https://docs.riscv.org/reference/isa/unpriv/rv32.html#integer-computational-instructions)）。
- RISC-V 暫存器移位只看低 `log2(XLEN)` 位；RV32 即低 5 位（來源事實：[RISC-V 非特權規格，RV32I Register-Register Operations](https://docs.riscv.org/reference/isa/unpriv/rv32.html#integer-register-register-operations)）。
- `JAL` 把下一指令位址寫入 `rd`；QEMU 實作在跳轉前以目前指令長度產生 successor PC。`JALR` 以 `rs1 + imm` 為目的地並清最低位（來源事實：[`target/riscv/insn_trans/trans_rvi.c.inc:141-165`](../../../target/riscv/insn_trans/trans_rvi.c.inc#L141-L165)）。這是成熟先例，不代表 Q32 必須清最低位。
- QEMU 的 RISC-V translator 對沒有 16-bit 指令的情況檢查 4-byte 對齊目的地，失敗則產生 instruction-address-misaligned（來源事實：[`target/riscv/translate.c:619-632`](../../../target/riscv/translate.c#L619-L632)）。

### 仍需人類決定

- `ADD/SUB` 是否只 wrap，或另設 overflow trap／flag；shift count 超過 31 是遮罩、非法或指定結果。
- branch base 是目前 PC 還是下一 PC；jump link 必須是 `PC+4`；間接跳轉是否清低位。
- branch/jump 目標未 4-byte 對齊時，在指令本身立刻 fault，或取指時 fault；illegal instruction 的 fault PC 與 signal code。

### 建議答案

**推論／建議：**採 RV32I 的無旗標 modulo-32 算術、shift count `& 31`、signed 與 unsigned 比較各自明列；所有 PC-relative displacement 以當前指令 PC 為基準，link=`PC+4`。因 Q32 永遠固定 4-byte，所有 taken branch/jump 目標要求 4-byte 對齊；不對齊在該控制指令報 fault，且 PC 保持 faulting instruction。所有未匹配或保留編碼以同一 illegal-instruction trap 送 `SIGILL`。

## 定義 Q32 記憶體存取語意

### 已知約束

- Q32 已決定 little-endian，並有 8/16/32-bit load/store；尚未決定 misalignment、fault 原子性與 ordering。
- QEMU `MemOp` 能攜帶 alignment 要求，guest virtual access 可觸發 alignment/MMU fault；load/store API 也明列 `_le` 形式（來源事實：[`docs/devel/loads-stores.rst:73-98`](../../../docs/devel/loads-stores.rst#L73-L98)、[`docs/devel/loads-stores.rst:106-121`](../../../docs/devel/loads-stores.rst#L106-L121)）。

### 可採先例

- RISC-V 規格要求自然對齊 load/store 必須成功；misaligned 是否完全支援、極慢或產生 address-misaligned/access-fault，取決於 execution environment（來源事實：[RISC-V 非特權規格，Load and Store Instructions](https://docs.riscv.org/reference/isa/unpriv/rv32.html#load-and-store-instructions)）。Q32 必須自行凍結，不應留下 environment-dependent。
- RISC-V base memory model 保證同一 hart 的 load/store 依程式順序被觀察，但跨 hart 順序由 RVWMO/FENCE 等定義；Q32 v1 為 linux-user 且排除 atomics，仍需描述單執行緒可觀察順序與 self-modifying code 邊界（來源事實：[RISC-V RVWMO](https://docs.riscv.org/reference/isa/unpriv/rvwmo.html)）。

### 仍需人類決定

- 16/32-bit misaligned access 是成功、`SIGBUS`，或依頁面分段；store 跨頁第二段 fault 時是否允許部分寫入。
- 32-bit effective address 計算是否 modulo \(2^{32}\)；跨 `0xffffffff` 的多 byte access 是 wrap 還是 fault。
- load fault 是否保留目的暫存器；store fault 的架構可見副作用；instruction/data coherence 是否需要 fence 指令。

### 建議答案

**推論／建議：**v1 僅允許自然對齊，misaligned 16/32-bit access 在存取前檢查並送 `SIGBUS`，不允許部分 store；byte access 永遠對齊。有效位址加法以 32-bit modulo 計算，但任何多 byte access 若跨越 `0xffffffff` 則 fault、不可繞回 0。load 以 little-endian 組合，`LB/LH` 符號擴展、`LBU/LHU` 零擴展；fault 時不改目的暫存器。v1 只承諾單一 guest thread 的 program order，不宣稱多執行緒記憶體模型；若要支援共享記憶體並發，必須另開 ISA 決策而不能默認 host ordering。

## 完成 Q32 ELF 與行程 ABI

### 已知約束

- Q32 是 ELF32、little-endian、ILP32，既定 register convention 為 `r1=ra`、`r2=sp`、`r10–r17` 參數／結果，stack 16-byte alignment；syscall number 在 `r17`，六參數在 `r10–r15`。
- QEMU target ELF header 以 `ELF_MACHINE` 與 `ELF_CLASS` 約束 loader；RISC-V 32-bit 直接宣告 `EM_RISCV`、`ELFCLASS32`，core register set 是 PC 加 x1..x31（來源事實：[`linux-user/riscv/target_elf.h:11-28`](../../../linux-user/riscv/target_elf.h#L11-L28)）。ELF 的 class/data 常數為 `ELFCLASS32=1`、`ELFDATA2LSB=1`（來源事實：[`include/elf.h:1669-1674`](../../../include/elf.h#L1669-L1674)）。
- RISC-V linux-user 實作正以 a7/a0..a5 傳 syscall，restart 時 PC 回退 4，sigreturn/ESETPC 不覆寫 a0（來源事實：[`linux-user/riscv/cpu_loop.c:48-70`](../../../linux-user/riscv/cpu_loop.c#L48-L70)）。

### 可採先例

- RISC-V psABI 的 ILP32 register convention、16-byte stack alignment、caller/callee saved 表格與 initial stack 規則可作 Q32 模板（來源事實：[RISC-V psABI，Calling Convention](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#_calling_convention)、[Procedure Calling Convention](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#procedure-calling-convention)）。
- System V gABI 定義 ELF header、program header、symbol、relocation 的通用契約；processor-specific `e_machine` 與 relocation types 必須由處理器 ABI 補充（來源事實：[ELF Header](https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html)、[Relocation](https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.reloc.html)）。
- QEMU 的 clone hooks 把 child return 設 0、可換 SP，TLS 寫入指定 TP register（來源事實：[`linux-user/riscv/target_cpu.h:4-25`](../../../linux-user/riscv/target_cpu.h#L4-L25)）。signal frame 會保存 PC/GPR/mask、16-byte 對齊，handler 收 signal/info/ucontext 三參數，trampoline 需能載入 `rt_sigreturn` number 並 `ecall`（來源事實：[`linux-user/riscv/signal.c:62-77`](../../../linux-user/riscv/signal.c#L62-L77)、[`linux-user/riscv/signal.c:114-136`](../../../linux-user/riscv/signal.c#L114-L136)、[`linux-user/riscv/signal.c:183-215`](../../../linux-user/riscv/signal.c#L183-L215)）。

### 仍需人類決定

- 暫定私有 `e_machine`、`e_flags`、OSABI；動態連結是否排除；最小產生器需要哪些 relocation。
- `r3–r9`、`r18–r31` 的完整 caller/callee saved 分配，以及 frame pointer、thread pointer；small aggregate、64-bit scalar、varargs 的傳遞規則。
- initial stack 的 `argc/argv/envp/auxv` 精確 layout；signal `ucontext` byte layout、signal codes、TLS/clone；errno 使用負值直返的完整範圍。

### 建議答案

**推論／建議：**完整採 RISC-V ILP32 的角色分配並映射到同號 Q32 暫存器（含 `r4=tp`），但文件明稱 Q32 ABI。v1 ELF 限 `ET_EXEC`、static、無 dynamic linker；產生器第一階段不使用 relocation，若需要符號組裝則只定義 `R_Q32_32`、`R_Q32_PC32` 兩種內部 relocation。選一個明確標示「測試私用、不得發布」的暫定 `e_machine`，三處保持一致。initial stack、auxv、signal frame 與 clone/TLS 必須在 ABI 票內列成位元組級表格後才能關票。

## 選擇 Q32 QEMU target 的模組邊界

### 已知約束

- 既有研究已確定以 OpenRISC 作 target 骨架、RISC-V 作 linux-user ABI 路徑；本節只補充邊界判準。
- QEMU 的 guest memory helper 可能 longjmp 離開 generated code，因此 fault 前的架構狀態同步與 `retaddr` 使用是 translator/helper 的界面責任（來源事實：[`docs/devel/loads-stores.rst:87-104`](../../../docs/devel/loads-stores.rst#L87-L104)）。

### 可採先例

- OpenRISC 的 Meson 把共用 translator/CPU、user-only GDB、system-only MMU/interrupt/machine 分成 source sets（來源事實：[`target/or1k/meson.build:1-31`](../../../target/or1k/meson.build#L1-L31)）。
- QEMU translator 必須以 `insn_start` 留下可恢復 guest PC，並在 `TranslatorOps` 實作 context 初始化、逐指令翻譯與 TB stop；OpenRISC 固定 4-byte 流程是直接先例（來源事實：[`target/or1k/translate.c:1519-1567`](../../../target/or1k/translate.c#L1519-L1567)、[`target/or1k/translate.c:1636-1651`](../../../target/or1k/translate.c#L1636-L1651)）。

### 仍需人類決定

- disassembler 與 GDB stub 是 v1 必備或後續；helper 使用門檻；signal ABI 由 `linux-user/q32` 擁有、CPU trap enum 由 `target/q32` 擁有的明確介面。
- 是否讓 generated decoder 同時服務 translator 與 disassembler；公共 header 的最小暴露面。

### 建議答案

**推論／建議：**`target/q32` 只擁有架構狀態、decode/translate、trap 產生、dump/disassembly；`linux-user/q32` 獨占 ELF、syscall、signal、clone/TLS。簡單 ALU/load/store 全部直接產 TCG ops，只有不可自然表示或需精確 C fault 的操作才用 helper。v1 把 disassembler 列為必備診斷能力，GDB stub 可延後但要明列非目標；禁止建立任何 system-mode 空殼。

## 設計 Q32 測試產物工具

### 已知約束

- 正式 GCC/LLVM/binutils 不在範圍；工具必須可重現地生成 instruction bytes 與 ELF，並能生成負面案例。
- QEMU TCG 測試框架由 target `Makefile.target` 擴充 `TESTS`，`run` 目標執行所有 `RUN_TESTS`，且支援 target-specific run rules（來源事實：[`tests/tcg/Makefile.target:84-111`](../../../tests/tcg/Makefile.target#L84-L111)、[`tests/tcg/Makefile.target:142-155`](../../../tests/tcg/Makefile.target#L142-L155)、[`tests/tcg/Makefile.target:259-278`](../../../tests/tcg/Makefile.target#L259-L278)）。

### 可採先例

- `tests/tcg/hexagon/Makefile.target` 展示 target 專屬測試清單、startup 物件與 run 規則；Q32 可替換交叉編譯步驟為產生器（來源事實：[`tests/tcg/hexagon/Makefile.target`](../../../tests/tcg/hexagon/Makefile.target)）。
- ELF gABI 指定 `Elf32_Ehdr`、`Elf32_Phdr` 的欄位與載入 segment 規則，可直接作 deterministic writer 的資料模型（來源事實：[ELF Header](https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html)、[Program Header](https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.pheader.html)）。

### 仍需人類決定

- 輸入語法是 assembly-like DSL、Python API 或宣告式資料；是否支援 labels/expressions；golden bytes 如何審核。
- ELF 固定位址與 layout、是否刻意生成 malformed headers、expected result 如何表達（stdout、exit、signal、register trace）。

### 建議答案

**推論／建議：**建立一個無外部套件依賴的 Python 命令列產生器，輸入為可審查的 YAML/JSON 不如簡單 assembly DSL；建議 DSL 支援 label、`.word/.byte/.align`、指令助記符與 `expect exit|signal|stdout`。輸出 byte-for-byte deterministic 的單一 static ELF，另提供 `--raw` 與 `--mutate elf-field=value` 生成負面案例。每個測試同時提交來源與短小 golden hexdump；產生器 `--check` 必須重新生成後比較，不得把手工 blob 當唯一真相。

## 設定 Q32 驗證與實作交接標準

### 已知約束

- Wayfinder 終點是無未決架構問題的可實作設計，不是 target 實作；驗收必須同時證明規格完整、模組邊界可落地、測試 oracle 可重現。
- QEMU TCG 框架原生區分普通 `TESTS`、額外測試與 `run-*`，也可用 `TCG_TEST_FILTER` 過濾（來源事實：[`tests/tcg/Makefile.target:142-155`](../../../tests/tcg/Makefile.target#L142-L155)、[`tests/tcg/Makefile.target:259-278`](../../../tests/tcg/Makefile.target#L259-L278)）。

### 可採先例

- decodetree 未匹配模式回到 translator 的非法指令路徑，可將「所有保留 encoding 均 SIGILL」變成機器可驗證的 negative matrix（來源事實：[`docs/devel/decodetree.rst:176-207`](../../../docs/devel/decodetree.rst#L176-L207)）。
- linux-user RISC-V loop 展示 syscall restart、sigreturn、illegal instruction、debug signal 的不同狀態路徑，不能只用 `write+exit` smoke 覆蓋（來源事實：[`linux-user/riscv/cpu_loop.c:48-93`](../../../linux-user/riscv/cpu_loop.c#L48-L93)）。
- QEMU 測試框架會把多架構測試與 target-specific 測試合併；沒有 target startup 支援時會跳過相應 multiarch 集合（來源事實：[`tests/tcg/Makefile.target:102-136`](../../../tests/tcg/Makefile.target#L102-L136)）。

### 仍需人類決定

- 「交接完成」是否要求 GDB、disassembler、signal/clone/TLS 全部在第一實作波；效能與 fuzzing 是否是 gate。
- 規格與生成器誰是 encoding 的單一真相，以及修改 ABI 的版本化規則。

### 建議答案

**推論／建議：**交接包需包含：(1) 完整 opcode/format 表與逐指令 pseudocode；(2) ABI 的 register、stack、ELF、syscall、signal/TLS 位元組級表；(3) QEMU 檔案／介面清單與依賴順序；(4) 可重現產生器契約；(5) 驗收矩陣。最低測試 gate 分五層：target-only build；ELF accept/reject；每個合法指令與保留編碼；算術/branch/memory 邊界與精確 fault PC；syscall restart、signal round-trip、clone/TLS。每個案例都必須定義輸入 bytes、初始狀態、預期 stdout/exit/signal/最終 state。性能、system-mode、正式 toolchain、atomics 與 fuzzing 明列為非 gate。

## 跨票結論

第一手證據足以支持一條保守且低風險的路線：用 RV32I 的固定欄位與無旗標整數語意作模板、把 Q32 的未對齊與跨位址頂端行為定得比 RISC-V execution-environment 選項更嚴格、以 RISC-V ILP32/linux-user 契約補齊 signal/TLS/clone，並用 deterministic ELF 產生器接入 QEMU TCG 測試。它們都只是成熟先例；Q32 在關閉前四張票時仍必須把上述「仍需人類決定」逐項寫成自己的規範，尤其不能以「類似 RISC-V」代替 ABI 或 ISA 定義。
