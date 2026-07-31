# 定義 Q32 記憶體存取語意

Type: grilling
Status: resolved

## 問題

Q32 v1 的 8、16 及 32 位元載入與儲存應採用哪些對齊、符號／零擴展、位址環繞、錯誤及記憶體順序規則？

## 答案

### 有效位址與對齊

- 有效位址為 `(rs1 + sign_extend(imm12)) mod 2^32`，加法只保留低 32 位元且不產生溢位例外。
- 8 位元存取可使用任何位址；16 位元存取要求 `address & 1 == 0`；32 位元存取要求 `address & 3 == 0`。
- 未對齊時，在讀寫記憶體前產生資料位址未對齊陷阱：load 不修改目的暫存器，store 不寫入任何位元組。linux-user 映射為 guest `SIGBUS`。
- 多位元組存取不得跨越 `0xffffffff` 後繞回位址 0。在自然對齊規則下，合法的 16／32 位元存取不會跨越位址頂端。

### 小端序、擴展與截斷

- 最低有效位元組位於最低記憶體位址。
- `LB/LH` 分別載入 8／16 位元並符號擴展至 32 位元；`LBU/LHU` 零擴展；`LW` 載入完整 32 位元。
- `SB/SH` 只儲存來源暫存器的低 8／16 位元；`SW` 儲存完整 32 位元。
- load 的 `rd=r0` 時仍必須執行實際存取及所有 fault 檢查，只丟棄成功讀取的結果。

### 存取錯誤與精確狀態

- 已對齊但因位址未映射、頁面不存在或權限不足而失敗時，產生架構級資料存取錯誤，保存造成錯誤的 load/store 指令 PC。
- load fault 不修改目的暫存器；store fault 不允許部分寫入。linux-user 將資料存取錯誤映射為 guest `SIGSEGV`；`SEGV_MAPERR` 與 `SEGV_ACCERR` 的區分由行程 ABI 定義。
- 每個自然對齊的 8／16／32 位元 load/store 都是單一、不可分割的架構動作。非同步 signal 只能觀察該指令完成前或完成後的狀態；此保證不構成多執行緒同步原子操作。

### 可見順序與程式碼修改

- Q32 v1 只承諾單一 guest thread；同一執行緒的 load/store 按程式順序觀察。
- 成功的 store 對同一執行緒後續 load 立即可見。若 store 修改可執行記憶體，後續取指也必須看到新內容，不需要額外的 cache 或 instruction fence。
- QEMU 實作在寫入已翻譯程式碼頁時必須使相關 translation block 失效。
- Q32 v1 不定義多執行緒共享記憶體模型，也不提供 atomic、fence 或 read-modify-write 指令；以 `clone` 建立共享記憶體執行緒不屬於 v1 必備能力。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#定義-q32-記憶體存取語意)。
