# 選擇 Q32 QEMU target 的模組邊界

Type: grilling
Status: resolved
Blocked by: 02, 03, 04

## 問題

應如何劃分 Q32 CPU 狀態、解碼流程、TCG 翻譯、例外流程、linux-user ABI 銜接層及 target 對外介面，使實作完整、易於維護並符合 QEMU 慣例？

## 答案

### 頂層責任

- `target/q32` 以 OpenRISC 的目錄結構為參考，只擁有 CPU/QOM、架構狀態、decode/translate、架構陷阱、state dump 與 disassembly。
- `linux-user/q32` 以 RISC-V 32 linux-user 流程為參考，只擁有 ELF/process 初始化、syscall、signal frame、process clone/fork hooks，以及陷阱至 Linux signal 的映射。
- 只建立 `q32-linux-user`；不建立 softmmu、machine、MMU、interrupt、migration 或 system-mode 空殼。
- translator 不得直接呼叫 Linux syscall 或建立 signal frame；linux-user 不得依賴 translator 私有資料結構。

### `target/q32/` 檔案

- `cpu-qom.h`：QOM 型別與 cast macro。
- `cpu-param.h`：32-bit VA 與 4 KiB page。
- `cpu.h`：`CPUArchState`、`ArchCPU`、陷阱與公開介面。
- `cpu.c`：QOM、reset、PC/state hooks 與 `TCGCPUOps`。
- `translate.c`：TCG globals、`TranslatorOps` 與 `trans_*` callback。
- `insns.decode`：Q32 v1 的唯一 decodetree pattern 來源。
- `disas.c`：使用相同 decoder 的反組譯。
- `meson.build`：decoder 產生與 q32 source set。

v1 僅建立 `helper.h` 與例外拋出實作，提供 translator 經由 TCG helper
設定 `CPUState.exception_index` 並離開 CPU loop；不放入任何 ISA 運算語意。
不建立 `mmu.c`、`interrupt.c`、`machine.c` 或 VMState。

### CPU state 與 model

`CPUArchState` 只包含 `uint32_t gpr[32]`、`uint32_t pc`、`uint32_t badaddr`。

- `gpr[0]` 儲存槽存在，但所有入口維持為零；`badaddr` 保存 alignment fault address。
- 例外種類只存於 `CPUState.exception_index`。
- 唯一 CPU model 是 `q32-v1`；reset 清零全部狀態，ELF loader 再設定 PC/SP。
- 不包含 flags、CSR、privilege、MMU、FPU、interrupt state。

### 例外邊界

只定義 `Q32_EXCP_SYSCALL`、`Q32_EXCP_ILLEGAL`、`Q32_EXCP_ALIGN`：

- `ECALL` 產生 syscall；未匹配／保留編碼產生 illegal；取指或資料未對齊先寫 `badaddr` 再產生 align。
- 一般未映射／權限 TCG memory fault 沿用 QEMU user-mode 共用 `SIGSEGV` 路徑，不重包為 Q32 enum。
- cpu loop 另處理 QEMU 共用 `EXCP_INTERRUPT` 與 `EXCP_DEBUG`；Q32 v1 不定義 `EBREAK`。

### Decoder 與 disassembler

- Meson 對 `insns.decode` 執行一次 `decodetree.process`；`translate.c` 與 `disas.c` 各自 include generated decoder 並提供各自的同名 `trans_*` callbacks。
- translator 未匹配時產生 illegal；disassembler 未匹配時顯示 `.word 0x????????`。
- 固定擷取 4-byte little-endian word；disassembly 使用 ABI register names，state dump 顯示 `rN`。
- disassembler 是 v1 必備診斷能力。GDB stub 與 XML register description 延後，不阻擋 bring-up。

### TCG 與 TB

- 建立 `cpu_gpr[1..31]`、`cpu_pc`、`cpu_badaddr` 的 `TCGv_i32` globals；`r0` 讀取使用常數零，寫入不產生 op。
- load 到 `r0` 仍產生 memory load 並丟棄結果，以保留 fault。
- `insn_start` 記錄每條指令 PC；`get_tb_cpu_state` 只輸出 PC、flags=0；實作 `synchronize_from_tb` 與 `restore_state_to_opc`。
- `mmu_index` 固定為 user index 0。
- ALU、比較、shift、U-format、control flow、load/store 全部直接產 TCG ops。load/store 使用 little-endian `MemOp`，並在存取前產生自然對齊檢查。
- 對齊失敗先寫 `badaddr` 再 exception exit；`ECALL` 與 illegal 直接 exception exit。
- 同頁直接跳躍使用 `goto_tb + exit_tb`；間接、跨頁或 TB 終止時寫回 PC 並返回 dispatcher。

### `linux-user/q32/` 檔案

- `cpu_loop.c`：cpu_exec、syscall、trap→signal、main thread。
- `signal.c`：rt frame、sigreturn、trampoline。
- `elfload.c`：CPU model 與 core register copy。
- `target_cpu.h`：clone/fork、SP、TLS 拒絕規則。
- `target_elf.h`：ELF machine/class 與 gregset。
- `target_signal.h`：generic signal 與 sigtramp page。
- `target_syscall.h`：ABI types/hooks。
- `target_*` headers：QEMU generic 32-bit 定義的薄封裝。
- `syscall.tbl`、`syscallhdr.sh`、`meson.build`：generic syscall 與 header generator。

不複製 RISC-V 的 vDSO、浮點、semihosting 或架構專用 syscall。

### 建置整合

- 新增 `configs/targets/q32-linux-user.mak`：`TARGET_ARCH=q32`、`TARGET_LONG_BITS=32`、`TARGET_SYSTBL=syscall.tbl`、`TARGET_SYSTBL_ABI=common,32,memfd_secret`、`TARGET_NOT_USING_LEGACY_NATIVE_ENDIAN_API=y`；不設定 `TARGET_BIG_ENDIAN`。
- `target/meson.build` 與 `linux-user/meson.build` 加入 q32 subdir。
- `target/q32/meson.build` 只註冊 `target_arch += {'q32': q32_ss}`。
- `include/elf.h` 增加實驗性 `EM_Q32=0xFF32`。
- `--target-list=q32-linux-user` 必須可單獨 configure/build。

### CPU hooks

`CPUClass` 必備 `reset_hold`、`set_pc`、`get_pc`、`dump_state`、`disas_set_info`。

`TCGCPUOps` 必備 `initialize`、`translate_code`、`get_tb_cpu_state`、`synchronize_from_tb`、`restore_state_to_opc`、`mmu_index`。不提供 system `tlb_fill`、interrupt execution、migration、architecture `has_work` 或 MMU mode switching。

來源與先例見 [Q32 QEMU linux-user target 研究](../research-qemu-linux-user-target.md)及 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#選擇-q32-qemu-target-的模組邊界)。
