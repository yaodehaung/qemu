# 定義 Q32 並設計其 QEMU user-mode target

Label: wayfinder:map

## 終點

完成一份最小但完整的 Q32 v1 ISA 規格，以及可直接交付實作的 QEMU user-mode target 設計與工作拆解，使其能執行 Q32 ELF 測試程式。本地圖的終點是規劃與規格，不包含實作 target。

## 備註

- 解決設計決策時使用 `grilling` 與 `domain-modeling` 技能。
- Q32 v1 是 32 位元、小端序架構，使用固定 32 位元長度的指令。
- 它是載入／儲存式 RISC，具有 32 個 32 位元通用暫存器、恆為零的 `r0`，以及獨立的程式計數器。
- 函式呼叫慣例採用類似 RISC-V ILP32 的形式：`r1` 是返回位址、`r2` 是堆疊指標、`r10`–`r17` 傳遞參數與結果，並明確定義 caller-saved 與 callee-saved 暫存器；堆疊按 16 位元組對齊。
- 使用 Linux generic syscall 編號。`r17` 保存系統呼叫編號，`r10`–`r15` 傳遞最多六個參數，`r10` 保存結果，並由 `ecall` 發出系統呼叫請求。
- Q32 v1 包含整數暫存器／立即數算術、位元運算、比較、條件分支、跳躍、8／16／32 位元載入與儲存、系統呼叫陷阱及非法指令陷阱。
- 小型組譯器或 ELF 測試產生器在範圍內；GCC 與 LLVM 後端不在範圍內。

## 目前決策

- [確定 Q32 v1 指令編碼](issues/01-fix-instruction-encoding.md) — 採用 RV32I 同形的六種固定欄位格式、Q32 自有 opcode、word-scaled 分支／跳躍位移，以及全域保留編碼非法政策。
- [定義 Q32 整數與控制流程語意](issues/02-define-integer-and-control-semantics.md) — 採模 2^32 整數運算、低 5 位移位量、精確陷阱 PC、word-scaled 控制流程，以及清除最低兩位的 JALR 目標。
- [定義 Q32 記憶體存取語意](issues/03-define-memory-semantics.md) — 採自然對齊的小端存取、精確 SIGBUS/SIGSEGV 邊界、單執行緒程式順序，以及後續取指可見的自修改程式碼。
- [完成 Q32 ELF 與行程 ABI](issues/04-complete-elf-and-process-abi.md) — 定義私有靜態 ELF32、modern generic syscall、ILP32 register/stack、精確 signal frame，以及單行程 process ABI。
- [選擇 Q32 QEMU target 的模組邊界](issues/05-choose-qemu-target-boundaries.md) — 採 OpenRISC 式 user-only target 骨架、RISC-V 式 linux-user ABI 路徑、共用 decodetree、直接 TCG ops 與嚴格 target/process 分層。
- [設計 Q32 測試產物工具](issues/06-design-test-artifact-tooling.md) — 經互動原型確認採無依賴 Python assembly DSL、deterministic static ELF、顯式 expectation/mutation 與純函式核心。
- [設定 Q32 驗證與實作交接標準](issues/07-set-verification-and-handoff-criteria.md) — 以五層 gate、完整邊界矩陣、六個 TDD 工作套件及 Standards/Spec 雙軸 review 定義完成條件。

## 尚未具體化

無；通往終點的決策均已完成。


## 範圍外

- QEMU system-mode 支援、機器／機板模型、特權模式、MMU 行為及中斷硬體。
- GCC 或 LLVM 編譯器後端，以及可供正式使用的 binutils 移植。
- 浮點、向量、原子操作、壓縮指令及 ISA 擴充機制。
- 多執行緒共享記憶體模型、同步指令，以及以 `clone` 建立共享記憶體執行緒。
- 動態 ELF、shared library、vDSO、ELF relocation，以及正式 linker ABI。
- 64 位元純量、aggregate、varargs 與結構返回的函式 ABI。
- Q32 v1 的 GDB remote stub 與 XML register description。
- 向上游申請正式的 ELF 或 Linux 架構識別碼。
