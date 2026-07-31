# 定義 Q32 整數與控制流程語意

Type: grilling
Status: resolved
Blocked by: 01

## 問題

每一條 Q32 v1 整數、比較、分支、跳躍及陷阱指令的精確架構語意為何，包括溢位、移位寬度、連結位址、PC 相對定址及非法指令行為？

## 答案

### 整數與位元運算

- 所有 32 位元加減、PC、位址及 `AUIPC` 計算均採模 \(2^{32}\) 環繞，不產生算術溢位例外；Q32 v1 不設狀態旗標。
- `ADD/ADDI` 計算加法，`SUB` 計算減法；`XOR/XORI`、`OR/ORI`、`AND/ANDI` 逐位運算。I-format 操作數先把 `imm12` 符號擴展至 32 位元。
- `SLL/SRL/SRA` 只使用 `rs2 & 31`；立即數版本使用編碼中的 `imm[4:0]`。`SLL` 與 `SRL` 是邏輯移位，`SRA` 是 32 位元二補數算術右移；移位 0 位保留原值。
- `SLT/SLTI` 使用 32 位元二補數有號比較；`SLTU/SLTIU` 使用無號比較。成立寫 `1`，否則寫 `0`。`SLTIU` 先符號擴展 `imm12`，再把該 32 位元位元模式視為無號數。
- `LUI` 寫入 `imm20 << 12`；`AUIPC` 寫入 `instruction_PC + (imm20 << 12)`。

### `r0`

- 讀取 `r0` 永遠得到零；所有架構路徑對 `r0` 的寫入均丟棄。
- 此規則涵蓋 ALU、load、`LUI`、`AUIPC`、`JAL/JALR`、syscall 返回、reset、signal restore 與 debugger 寫入。

### PC 與條件分支

- 一般指令及未採用的分支完成後，`PC = instruction_PC + 4`。
- 採用的分支以目前指令 PC 為基準：`PC = instruction_PC + sign_extend(word_offset << 2)`。
- `BEQ/BNE` 比較完整 32 位元是否相等；`BLT/BGE` 使用有號比較；`BLTU/BGEU` 使用無號比較。
- 所有 PC 計算採模 \(2^{32}\) 環繞。B/J 位移採 4-byte word scaling，因此直接分支與跳躍目標天然對齊。

### 跳躍與連結

- `JAL` 的目標為 `instruction_PC + sign_extend(word_offset << 2)`，連結值為 `instruction_PC + 4`。
- `JALR` 先計算 `rs1 + sign_extend(imm12)`，採模 \(2^{32}\) 環繞，再以 `& 0xfffffffc` 清除最低兩位；連結值同樣是 `instruction_PC + 4`。
- `rd != r0` 時在跳躍前寫入連結值；`rd = r0` 時丟棄連結值。

### 陷阱與精確狀態

- `ECALL` 保存該指令自身的 PC，且不先產生架構副作用。linux-user 在接到陷阱後才把 PC 推進 4；需要 restart 時回到原 `ECALL`。
- 未配置或保留編碼產生非法指令陷阱，保存錯誤指令自身的 PC，不提交任何副作用；linux-user 映射為 guest `SIGILL`。
- 每次取指前檢查 `PC & 3`。非零時產生指令位址未對齊陷阱，保存原 PC，不讀取指令記憶體且不產生副作用；linux-user 映射為 guest `SIGBUS`。
- 指令位址未對齊與非法指令是不同陷阱：前者未取指，後者已成功取到 32 位元指令但無法解碼。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#定義-q32-整數與控制流程語意)。
