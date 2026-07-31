# 確定 Q32 v1 指令編碼

Type: grilling
Status: resolved

## 問題

應採用哪些位元級指令格式、操作碼空間、暫存器欄位、立即數編碼及保留／非法模式，才能為每項 Q32 v1 操作提供無歧義的固定 32 位元編碼，同時保留適當的未來發展空間？

## 答案

Q32 v1 採用固定 32 位元指令字與 7 位元主要 opcode。所有已配置 opcode 的最低兩位均為 `11`。暫存器欄位固定為 `rd[11:7]`、`rs1[19:15]`、`rs2[24:20]`，使用與 RV32I 同形的 R／I／S／B／U／J 六種格式，但使用 Q32 自有 opcode。

### 主要 opcode

| 類別 | Opcode |
|---|---:|
| `LOAD` | `0x0B` |
| `STORE` | `0x1B` |
| `OP-IMM` | `0x2B` |
| `OP` | `0x3B` |
| `BRANCH` | `0x4B` |
| `JALR` | `0x5B` |
| `JAL` | `0x6B` |
| `LUI` | `0x7B` |
| `AUIPC` | `0x0F` |
| `SYSTEM` | `0x1F` |

### 整數運算子碼

| 指令 | `funct3` | `funct7`／高位限制 |
|---|---:|---:|
| `ADD`／`ADDI` | `000` | `ADD=0000000`；`ADDI` 無 |
| `SUB` | `000` | `0100000` |
| `SLL`／`SLLI` | `001` | `0000000` |
| `SLT`／`SLTI` | `010` | `SLT=0000000`；`SLTI` 無 |
| `SLTU`／`SLTIU` | `011` | `SLTU=0000000`；`SLTIU` 無 |
| `XOR`／`XORI` | `100` | `XOR=0000000`；`XORI` 無 |
| `SRL`／`SRLI` | `101` | `0000000` |
| `SRA`／`SRAI` | `101` | `0100000` |
| `OR`／`ORI` | `110` | `OR=0000000`；`ORI` 無 |
| `AND`／`ANDI` | `111` | `AND=0000000`；`ANDI` 無 |

移位立即數只使用 `imm[4:0]`；`imm[11:5]` 必須符合表中的限制。

### 記憶體與分支子碼

| 類別 | 指令與 `funct3` |
|---|---|
| 載入 | `LB=000`、`LH=001`、`LW=010`、`LBU=100`、`LHU=101` |
| 儲存 | `SB=000`、`SH=001`、`SW=010` |
| 分支 | `BEQ=000`、`BNE=001`、`BLT=100`、`BGE=101`、`BLTU=110`、`BGEU=111` |

`JALR` 只允許 `funct3=000`。`LUI` 與 `AUIPC` 使用 U-format；`JAL` 使用 J-format。

### 立即數與位移

- I／S-format 使用 12 位元二補數有號立即數並符號擴展。
- U-format 把 20 位元立即數放入結果的 `[31:12]`，低 12 位補零。
- `LUI` 產生 `imm20 << 12`；`AUIPC` 產生 `PC + (imm20 << 12)`。
- B／J-format 保持 RV32I 同形欄位，但位移採 4-byte word scaling：編碼 `offset >> 2`，解碼時補回兩個最低零位元。位移以目前指令 PC 為基準。
- B-format 位元組範圍為 -8192 至 +8188；J-format 為 -2 MiB 至 +2 MiB - 4。

### 固定與保留編碼

- `ECALL` 是唯一的 Q32 v1 `SYSTEM` 指令，完整指令字為 `0x0000001F`。
- 標準 `NOP` 是 `ADDI r0, r0, 0` 的組譯別名，完整指令字為 `0x0000002B`。
- 所有未配置 opcode，以及已配置 opcode 內未定義的欄位組合，均為 `RESERVED`。
- 執行任何 `RESERVED` 編碼一律產生非法指令陷阱。Q32 v1 不提供自訂 opcode 機制；未來啟用保留空間必須提升 ISA 版本。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#確定-q32-v1-指令編碼)。
