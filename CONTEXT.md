# 領域詞彙表

## Q32

本次工作所設計的新型 32 位元、小端序、載入／儲存式 RISC 指令集架構。

## Q32 v1

Q32 的第一個最小版本。它足以執行整數型使用者程式，但不包含特權執行、浮點、向量、原子操作、壓縮指令及擴充機制。

## Q32 user program

包含 Q32 v1 指令，並採用 Q32 函式呼叫慣例及系統呼叫慣例的 ELF 程式。

## Q32 user-mode target

用來執行 Q32 使用者程式的 QEMU 執行環境。它不模擬機板、特權執行、MMU 或中斷硬體。

## Q32 v1 指令字

一個固定 32 位元的 Q32 v1 編碼單位。它使用 R／I／S／B／U／J 六種固定欄位格式及 Q32 自有的 7 位元主要 opcode。

## Q32 保留編碼

尚未配置的 opcode 或已配置 opcode 中未定義的欄位組合。Q32 v1 執行保留編碼時一律產生非法指令陷阱；保留編碼不是自訂擴充空間。

## Q32 精確陷阱

在陷阱發生時保存造成陷阱的指令位址，且不提交該指令的架構副作用。Q32 v1 的系統呼叫、非法指令與指令位址未對齊均遵守此規則。

## Q32 自然對齊存取

8 位元存取可位於任何位址；16 位元存取位址必須是 2 的倍數；32 位元存取位址必須是 4 的倍數。未對齊存取在讀寫前產生精確陷阱。

## Q32 v1 記憶體模型

只承諾單一 guest thread 依程式順序觀察記憶體。成功的 store 對後續資料讀取及指令擷取立即可見；不定義多執行緒共享記憶體同步。

## Q32 ABI v1

Q32 user program 與 QEMU linux-user 之間的靜態 ELF32、ILP32 暫存器／堆疊、generic syscall、signal 與行程狀態契約。它使用實驗性私有 `e_machine=0xFF32`，不代表正式上游 ABI。

## Q32 signal frame

在 signal handler stack 上保存 `siginfo`、`ucontext`、PC、r1–r31、signal mask 與 alternate stack 的 416-byte little-endian 區塊。frame 與其中的 CPU context 均按 16-byte 對齊。

## Q32 target 層

QEMU 中只負責 Q32 CPU state、指令解碼、TCG 翻譯、架構陷阱、state dump 與 disassembly 的 user-only 架構層。它不擁有 Linux process ABI。

## Q32 linux-user 層

QEMU 中負責 Q32 ELF/process 初始化、syscall、signal 與 process clone/fork 的 ABI 銜接層。它不擁有或繞過 Q32 指令翻譯語意。

## Q32 測試產物

由可審查的 assembly-like DSL 確定性生成的靜態 Q32 ELF。其 source 同時記錄預期 exit、signal 或 stdout，以及負面測試所需的顯式 ELF mutation。

## Q32 v1 完成 gate

實作必須通過 build、ELF、ISA、memory/fault 與 process ABI 五層驗證，並完成 Standards 與 Spec 雙軸 code review，才可宣稱 Q32 v1 完成。
