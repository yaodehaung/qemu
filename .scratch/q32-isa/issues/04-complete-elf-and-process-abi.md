# 完成 Q32 ELF 與行程 ABI

Type: grilling
Status: resolved
Blocked by: 01, 02, 03

## 問題

Q32 使用者程式應採用何種完整的 ELF 與行程 ABI，包括 ELF 標頭值、暫存器保存規則、參數／返回值規則、進入程式時的堆疊配置、測試產物所需的重定位、系統呼叫錯誤及可觀察的行程狀態？

## 答案

### ELF 識別與載入範圍

- Q32 ABI v1 使用 `ELFCLASS32`、`ELFDATA2LSB`、`EV_CURRENT`、`ELFOSABI_SYSV`、`ET_EXEC`。
- 暫定私有 `e_machine=0xFF32`，只供實驗測試，不代表正式上游分配；`e_flags` 必須為 0。
- 只支援靜態 `ET_EXEC`；不支援 `ET_DYN`、dynamic loader、shared library 或 vDSO。
- Q32 v1 不定義 relocation。`e_shoff=0`、`e_shnum=0`、`e_shstrndx=SHN_UNDEF`；測試工具直接輸出已完成配置的 executable。
- 使用標準 32-byte `Elf32_Phdr`，至少一個 `PT_LOAD`。`p_filesz <= p_memsz`，尾端補零；`p_vaddr`、`p_offset`、`p_align` 遵守 ELF 同餘規則。
- 允許必要的 `PF_R/PF_W/PF_X` 組合，包括自修改程式碼測試的 RWX。`e_entry` 必須 4-byte 對齊且位於可執行 segment。
- 拒絕大小溢位、32 位址環繞或互相衝突的 segment。
- target page 固定為 4096 bytes：`TARGET_PAGE_BITS=12`、`AT_PAGESZ=4096`，產生器預設 `p_align=4096`。

### 暫存器與函式 ABI

| 暫存器 | ABI 名稱 | 用途 | 保存責任 |
|---|---|---|---|
| `r0` | `zero` | 常數零 | — |
| `r1` | `ra` | 返回位址 | caller |
| `r2` | `sp` | 堆疊指標 | callee |
| `r3` | `gp` | 全域指標 | 固定 |
| `r4` | `tp` | thread pointer | 固定 |
| `r5–r7` | `t0–t2` | 暫存 | caller |
| `r8` | `s0/fp` | 保存值／frame pointer | callee |
| `r9` | `s1` | 保存值 | callee |
| `r10–r17` | `a0–a7` | 參數／返回值 | caller |
| `r18–r27` | `s2–s11` | 保存值 | callee |
| `r28–r31` | `t3–t6` | 暫存 | caller |

- v1 只定義 8／16／32 位元整數與 32 位元指標。前八個參數使用 `a0–a7`，其餘依序位於 entry `sp+0`、`sp+4`；小型別先依型別符號或零擴展。單一返回值在 `a0`。
- 不定義 64 位元純量、浮點、向量、aggregate、varargs 或結構返回。
- stack 向低位址成長，在每個 call boundary 保持 16-byte 對齊，不設 red zone。一般函式不得修改 `gp` 或 `tp`。

### 程式進入與初始堆疊

- 啟動時 `PC=e_entry`、`sp` 16-byte 對齊，其餘 GPR 為零；`r0` 強制為零，`gp/tp` 初始為零。
- stack 依序保存 32 位元 `argc`、`argv[]`、NULL、`envp[]`、NULL，再保存成對的 32 位元 `auxv(type,value)`。
- `auxv` 至少包含 `AT_PAGESZ`、`AT_PHDR`、`AT_PHENT`、`AT_PHNUM`、`AT_ENTRY`、`AT_UID/EUID/GID/EGID`、`AT_RANDOM`、`AT_EXECFN`，以 `AT_NULL,0` 結束。
- 字串與 16-byte `AT_RANDOM` 資料位於同一初始 stack image。

### Syscall ABI

- `a7` 保存 Linux generic syscall number，`a0–a5` 保存六個參數，`a6` 不使用；結果寫回 `a0`。
- 錯誤以 `-errno` 的 32 位元二補數返回；-1 至 -4095 為 errno 範圍，未知 syscall 返回 `-ENOSYS`。
- `ECALL` 進入 linux-user 後 PC 推進 4；restart 回復原 `ECALL` PC；sigreturn 或自行設定 PC 的內部結果不得再覆寫 `a0/PC`。
- syscall table 使用 `TARGET_SYSTBL_ABI=common,32,memfd_secret`，不包含 `riscv`、`time32` 或 legacy 相容標籤；`rt_sigreturn=139`。

### ILP32 kernel 資料模型

- `char=8`、`short=16`、`int/long/pointer=32`、`long long=64`，全部 little-endian。
- 32 位元 scalar/pointer 按 4-byte 對齊，64 位元 scalar 按 8-byte 對齊。`size_t/ssize_t/uintptr_t/intptr_t` 為 32 位元，modern `time_t` 與 time64 欄位為 64 位元。
- syscall、fcntl、mman、resource、socket 與 termios 等結構優先使用 QEMU generic 32-bit 定義；Q32 專屬目錄只擁有架構狀態相關結構與 hooks。

### Signal ABI

- signal frame 地址按 16-byte 對齊，共 416 bytes：offset `0x000` 是 128-byte `siginfo`，offset `0x080` 是 288-byte `ucontext`。
- `ucontext`：`uc_flags@0x000`、`uc_link@0x004`、12-byte `uc_stack@0x008`、8-byte `uc_sigmask@0x014`、120-byte reserved、12-byte alignment、128-byte `sigcontext@0x0a0`。reserved/alignment 建立時清零，恢復時忽略。
- `sigcontext` 依序保存 `PC@0x00` 與 `r1@0x04` 至 `r31@0x7c`；不保存 `r0`。
- handler entry：`a0=signal`、`a1=&siginfo`、`a2=&ucontext`、`sp=&rt_sigframe`、`ra=trampoline`、`PC=handler`。
- QEMU 配置一頁 RX、不可寫且位址不固定的 signal trampoline page；不提供 vDSO 或 `AT_SYSINFO[_EHDR]`。
- trampoline 是 little-endian `ADDI a7,r0,139`（`0x08B008AB`）與 `ECALL`（`0x0000001F`）。
- `rt_sigreturn` 還原 PC、r1–r31、mask 與 altstack；損壞 frame 產生 `SIGSEGV`。

### Trap、signal 與 process state

| 陷阱 | Signal | `si_code` |
|---|---|---|
| 保留／非法指令 | `SIGILL` | `ILL_ILLOPC` |
| 指令／資料位址未對齊 | `SIGBUS` | `BUS_ADRALN` |
| 未映射的取指／資料存取 | `SIGSEGV` | `SEGV_MAPERR` |
| 違反讀／寫／執行權限 | `SIGSEGV` | `SEGV_ACCERR` |

- 位址型 signal 的 `si_addr` 是 faulting guest address；非法指令使用 faulting instruction PC。
- core GPR set 為 32 個 little-endian word：word 0 是 PC，word 1–31 是 r1–r31；無浮點、向量或 CSR note。
- 啟動時 `tp=0`。thread-style `clone`（`CLONE_VM`／`CLONE_THREAD`）與 `CLONE_SETTLS` 返回 `-EINVAL`。
- process-style clone/fork 可支援：child `a0=0`、parent `a0=child pid`，可套用 new `sp`，其餘狀態從 syscall 完成點繼承。

來源與先例見 [Q32 七項設計決策的第一手證據](research-decision-evidence.md#完成-q32-elf-與行程-abi)及 [Q32 QEMU linux-user target 研究](../research-qemu-linux-user-target.md)。
