# 演算法分析 程式作業 2：BST、AVL Tree、Treap 與 Skip List 的實驗比較  
# Algorithm Analysis PA2: Experimental Comparison of BST, AVL Tree, Treap, and Skip List

本專案以模組化 **C++ (C++11 標準相容)** 實作四種動態搜尋資料結構：**二元搜尋樹 (BST)**、**參數化高度平衡樹 (AVL Tree, 上限 $\Delta$)**、**最小堆積隨機搜尋樹 (Treap, Min-Heap)** 與 **無層數上限跳躍串列 (Skip List, 機率 $p$)**，並提供完整正確性測試與三個實驗的自動化量測框架。

This project implements four dynamic search data structures in modular **C++ (C++11 compliant)** — **Unbalanced Binary Search Tree (BST)**, **Parameterized AVL Tree ($\Delta$)**, **Treap (Min-Heap on priority)**, and **Unbounded Skip List (probability $p$)** — along with a command-line framework for correctness verification and reproducible benchmarking across all three experiments.

---

## 1. 語言、編譯器與環境需求 / Language, Compiler & Environment

- **程式語言 / Language**: C++ (C++11 或更新標準 / C++11 or later, zero external dependencies)
- **建議編譯器與最佳化選項 / Recommended Compilers & Optimization Flags**:
  - **GCC (`g++`) / Clang (`clang++`)**: `-std=c++11 -O2 -Wall -Wextra -Wpedantic` (tested with GCC $\ge$ 9 / Clang $\ge$ 12)
  - **MSVC (`cl.exe`, Visual Studio 2022)**: `/EHsc /O2 /W4` (tested with MSVC v19.x x64)
- **計時器 / Timer**: `std::chrono::steady_clock`（單調高解析度時鐘 / monotonic high-resolution clock）

---

## 2. 專案檔案架構與核心函式對照 / File Structure & Function Mapping

| 檔案 / File | 說明 (繁體中文) | Description (English) |
|---|---|---|
| `DataStructures.h` | 四種資料結構類別與節點定義、後序解構子 (`~BST`, `~AVL`, `~Treap`, `~SkipList`) 與記憶體追蹤器 `MemoryTracker`。 | OOP implementations of `BST`, `AVL`, `Treap`, and `SkipList`, post-order destructors, and zero-leak `MemoryTracker`. |
| `Utils.h` | 共用亂數輸入產生器 (`GenerateInsertDataset`, `GenerateQueryDataset`)、`SteadyTimer` 與防 OOM 即時寫入 CSV 工具 (`AppendCSVRow`)。 | Shared dataset generators, `std::chrono::steady_clock` timer, SplitMix64 seed mixer, and incremental CSV writer. |
| `Experiments.h` | 五組手算正確性測試 + 自訂邊界測試 (`RunCorrectnessTests`)，以及實驗一、二、三與報告輔助統計表 (`RunExperiment1`–`3`)。 | Exact 5 PDF correctness tests + custom edge-case test, plus Experiments 1, 2, and 3 and report helper tables. |
| `main.cpp` | 指令列參數解析 (`--test`, `--exp`, `--trials`, `--seed`)。 | CLI entry point parsing `--test`, `--exp`, `--trials`, and `--seed`. |
| `Makefile` | Linux / macOS / WSL / MinGW 自動化編譯與執行腳本。 | Build & execution targets for `g++` environments. |

### 資料結構操作函式命名 / Explicit Data Structure Operations (`DataStructures.h`)
每一種資料結構皆提供明確標示結構名稱的操作函式（並將 `CreateXXX` 以物件導向建構子實作）：
Each class provides explicitly named member functions (with `CreateXXX` realized via constructors):

- **`BST`**: `BST()` / `BST(id, score)`, `InsertBST`, `PrintBST`, `HeightBST`, `SearchAVGBST`
- **`AVL`**: `AVL(delta)` / `AVL(id, score, delta)`, `InsertAVL`, `PrintAVL`, `HeightAVL`, `SearchAVGAVL`
- **`Treap` (Min-Heap)**: `Treap(seed)` / `Treap(id, score, seed)`, `InsertTreap` (含外部指定 `fixed_priority` 多載), `PrintTreap`, `HeightTreap`, `SearchAVGTreap`
- **`SkipList` (無層數上限 / Unbounded)**: `SkipList(p, seed)` / `SkipList(id, score, p, seed)`, `InsertSkipList` (含外部指定銅板字串如 `"HHT"` 或 `std::vector<bool>` 多載), `PrintSkipList`, `HeightSkipList`, `SearchAVGSkipList`

> **報告重點程式碼標記 / Report Highlight Markers**:  
> `DataStructures.h` 中已用 `=== [REPORT HIGHLIGHT: ...] ===` 註解清楚標出作業要求單獨說明的三個核心區段：
> 1. **AVL Tree** 判斷高度差超過 $\Delta$ 並決定單旋轉或雙旋轉的區段 (`InsertRecAVL`)
> 2. **Treap** 將新節點依 Min-Heap 性質往上旋轉的 `while` 迴圈 (`InsertImplTreap`)
> 3. **Skip List** 擲機率 $p$ 銅板決定新節點層數的 `while` 迴圈 (`InsertImplSkipList`)

---

## 3. 編譯方式 / Build Instructions

### Linux / macOS / WSL / MinGW (`g++` or `Makefile`)
```bash
make
# 或直接呼叫 g++ / Or compile directly:
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic main.cpp -o treebench
```

### Windows (Visual Studio Developer Command Prompt / PowerShell)
```powershell
cl /nologo /EHsc /O2 /W4 main.cpp /Fe:treebench.exe
```

---

## 4. 重現正確性測試與三張圖的執行指令 / Reproduction Commands

> Windows PowerShell 請將 `./treebench` 改為 `.\treebench.exe`（參數亦支援單破折號別名 `-exp`、`-trials`、`-seed`）。  
> On Windows PowerShell, use `.\treebench.exe` instead of `./treebench` (single-dash `-exp`, `-trials`, `-seed` are also accepted).

### (1) 正確性測試（五組手算測資 + 自訂雙旋轉與無上限測資 + 零記憶體洩漏檢查）
### Correctness Test Suite (5 Hand-Calculable PDF Sets + Custom Edge-Case Set + Leak Audit)
```bash
./treebench --test
```

### (2) 圖 1 — 實驗一：插入 $n$ 筆資料的平均總時間 ($n = 2^0 \dots 2^{20}$)
### Figure 1 — Experiment 1: Average Total Insertion Time ($n = 2^0 \dots 2^{20}$)
```bash
./treebench --exp 1 --trials 10 --seed 0
```
- **比較對象 / Structures**: `BST`, `AVL(delta=1)`, `Treap`, `SkipList(p=1/2)`
- **短區間校準 / Sub-millisecond Calibration**: 當單次建構時間 $< 1\text{ ms}$ 時，自動以同一份輸入建立 $r$ 個獨立結構一起計時再除以 $r$，並於停止計時後才釋放全部 $r$ 個結構。
- **輸出檔案 / Output**: `exp1_insert_time.csv` 以及整合原始數據檔 `raw_data.csv`。

### (3) 圖 2 — 實驗二：一次 `SearchAVG` 的平均搜尋時間 ($n = 2^0 \dots 2^{20}, Q = 100,000$)
### Figure 2 — Experiment 2: Average Time per `SearchAVG` ($n = 2^0 \dots 2^{20}, Q = 100,000$)
```bash
./treebench --exp 2 --trials 10 --seed 0
```
- **比較對象 / Structures**: `BST`, `AVL(delta=1)`, `Treap`, `SkipList(p=1/2)`
- **防最佳化與命中率統計 / Anti-`-O2` & Hit Ratio**: 將 $Q$ 次查詢回傳值加總並印出以防止編譯器刪除迴圈；計時結束後重跑同一組 $Q$ 筆查詢，統計未回傳 `-1`（查得到）的比例寫入 `exp2_hit_ratio.csv`（供報告實驗二第 2(a) 題使用）。
- **輸出檔案 / Output**: `exp2_search_time.csv`、`exp2_hit_ratio.csv` 以及 `raw_data.csv`。

### (4) 圖 3 — 實驗三：七個資料結構的平均高度 ($n = 2^0 \dots 2^{20}$) 與報告輔助統計表
### Figure 3 — Experiment 3: Average Height of 7 Structures ($n = 2^0 \dots 2^{20}$) & Report Tables
```bash
./treebench --exp 3 --trials 10 --seed 0
```
- **比較對象 / 7 Structures**: `BST`, `AVL(delta=1)`, `AVL(delta=5)`, `Treap`, `SkipList(p=1/2)`, `SkipList(p=3/4)`, `SkipList(p=1/4)`
- **附加分析輸出 / Report Helper Tables (`exp3_report_tables.txt`)**:
  1. $n = 2^{10} \dots 2^{20}$ 區間七條線的實測斜率 $\Delta h / \Delta \log_2 n$（第 1 題）
  2. $n = 2^{10}$ 遞增輸入 `id = 1..1024` 下七個結構的高度表（第 2 題）
  3. $\Delta = 1$ 與 $\Delta = 5$ 在 $2^{20}$ 節點的理論最高高度 $S_h$ 遞迴計算與實測平均旋轉次數（雙旋轉計為 2 次，第 3 題）
  4. 三種 $p \in \{1/4, 1/2, 3/4\}$ 在 $n = 2^{20}$ 時的平均高度與平均搜尋步數（往右一步 +1、往下一層 +1，獨立於實驗二計時函式外，第 4 題）
- **輸出檔案 / Output**: `exp3_height.csv`、`exp3_report_tables.txt` 以及 `raw_data.csv`。

### (5) 一次依序跑完三個實驗 / Run All Three Experiments Sequentially
```bash
./treebench --exp all --trials 10 --seed 0
```

---

## 5. 關鍵實作設計說明 / Key Design Decisions

1. **重複 ID 處理（每個 ID 單一節點 / One Node per ID）**  
   四種資料結構統一採用「每個相異 `id` 只建立一個節點」，節點內維護 `long long sum_score` 與 `int count`。插入已存在的 `id` 時直接原地累加總分與次數，不新增重複節點、亦不觸發額外旋轉；搜尋命中時在第一個符合的節點即可立即回傳 `(double)sum_score / count`，查無此 `id` 則回傳 `-1`。
   All four structures store one unique node per `id` (`sum_score`, `count`). Duplicate insertions update the node in place without structural changes, allowing successful `SearchAVG` queries to terminate immediately at the matching node (returning `-1` on miss).

2. **隨機種子控制與完全可重現性 / Deterministic Seed Derivation**  
   給定起始種子 `--seed S`，第 `t` 個 trial（`t = 0 .. trials-1`）以 `S + t` 結合資料量指數 $k = \log_2 n$ 經 SplitMix64 衍生出獨立的輸入種子、查詢種子與結構內部亂數種子（供 `Treap` 的 $(0,1)$ 優先度與 `SkipList` 的銅板使用）。同一個 trial 中所有資料結構皆拿到完全相同的輸入與查詢序列；相同 `--seed` 執行兩次會產生完全一致的結果與高度。
   Given `--seed S`, trial `t` derives deterministic sub-seeds from `S + t` and $k = \log_2 n$ via SplitMix64 for input generation, query generation, `Treap` priorities in $(0,1)$, and `SkipList` coin flips. All structures within a trial receive identical inputs/queries, and repeated runs with the same `--seed` are bit-for-bit reproducible.

3. **記憶體管理與防 OOM 資料保護 / Memory Management & Crash-Safe CSV Output**  
   - 每個 trial 結束時，資料結構皆透過嚴格的**後序走訪解構子 (Post-order Traversal Destructor)** 立即釋放所有動態配置節點，避免跨 trial 累積記憶體。
   - 每完成一個 $n$ 的 10 次平均，立即以 `std::ios::app` 寫入並 `flush` 至 CSV 檔（`raw_data.csv` 包含 `exp, log2_n, n, structure, unit, mean_value, trials` 欄位，可直接對照繪製圖 1～圖 3）。
