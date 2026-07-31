# 設計 Q32 測試產物工具

Type: prototype
Status: resolved
Blocked by: 01, 04

## 問題

最小的組譯器或 ELF 產生器介面與產物格式應如何設計，才能在不建立正式編譯器工具鏈的情況下，讓 Q32 指令、ABI、系統呼叫及整合測試易讀且可重現？

## 答案

經互動原型驗證，Q32 採用「assembly-like DSL + deterministic ELF writer + 顯式 expectation + 顯式 mutation」的單一無外部套件 Python 工具。

### 命令與檔案

- 正式工具置於 `tests/tcg/q32/q32-artifact.py`，以 `python3 tests/tcg/q32/q32-artifact.py SOURCE -o OUTPUT` 單一命令執行。
- 每個測試提交一份 UTF-8 DSL source；生成的 ELF 是 build artifact，不提交為唯一真相。
- `--check` 重新生成並 byte-for-byte 比較既有產物；相同 source 與工具版本必須產生完全相同 bytes。
- 工具只使用 Python 標準函式庫，不依賴 PyYAML、assembler、linker、GCC 或 LLVM。

### DSL

- 一行一個 label、指令或 directive；`#` 開始註解。
- 使用 Q32 ABI register names，也接受 `r0–r31`。
- 支援全部 Q32 v1 指令、十進位／十六進位立即數、向前／向後 labels，以及 `.byte`、`.word`、`.align`。
- `.text ADDRESS` 與 `.data ADDRESS` 建立 RX 與 RW `PT_LOAD`；預設 text base 為 `0x10000`、file offset 為 `0x1000`，page alignment 為 4096。
- label resolution 在產生 ELF 前完成；B/J 位移按 Q32 word scaling 檢查範圍與 4-byte 對齊。
- 不輸出 section table、symbol table 或 relocation；只產生 ABI 已定義的靜態 `ET_EXEC`。

### 預期結果

DSL 必須包含至少一個：

- `.expect exit NUMBER`
- `.expect signal NAME`
- `.expect stdout "TEXT"`

多個 expectation 可同時存在，例如 stdout 加 exit。runner 讀取同一 source 的 expectation，不另維護容易漂移的 shell oracle。

### 負面測試

`.mutate FIELD VALUE` 在正常 ELF 生成後做一項明確 mutation，至少支援：

- ELF class、endianness、`e_machine`、`e_flags`、entry alignment。
- program header 的 type、flags、offset/vaddr congruence、filesz/memsz、address wrap。
- raw instruction `.word`，用於保留／非法 encoding。

mutation 必須顯示在 source 中，不允許事後以任意 byte offset patch 隱藏測試意圖。

### 診斷與接入

- `--dump` 顯示 source line、guest address、32-bit word、ELF header/program headers 與 expectation。
- encode、label resolution 與 ELF build 是不含 I/O 的純函式；CLI 只負責讀寫與診斷。
- `tests/tcg/q32/Makefile.target` 先呼叫產生器，再以 `qemu-q32` 執行並比對 exit/stdout/signal。
- 產生器拒絕未知指令、超範圍立即數、未對齊 branch target、重複／未知 label，以及未指定 expectation 的 source。

### 原型證據

- Throwaway branch：`prototype/q32-test-artifact-tool`
- Commit：`94286d01e4a3e1427709402d04fd190add6741e5`
- 原型入口：`.scratch/q32-isa/prototypes/test-artifact-tool/tui.py`
- 驗證結論：使用者確認 DSL、expectation、mutation 與即時 ELF/state 呈現模型符合期待。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#設計-q32-測試產物工具)。
