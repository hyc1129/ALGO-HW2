# 演算法分析 程式作業 2：BST、AVL Tree、Treap 與 Skip List 的實驗比較

本專案以模組化 **C++ (C++11 標準相容)** 實作四種動態搜尋資料結構：**二元搜尋樹 (BST)**、**參數化高度平衡樹 (AVL Tree, 上限 $\Delta$)**、**最小堆積隨機搜尋樹 (Treap, Min-Heap)** 與 **無層數上限跳躍串列 (Skip List, 機率 $p$)**，並提供完整正確性測試、三個實驗的自動化量測框架與 **Python 繪圖腳本 (`plot.py`)**。

---

## 1. 語言、編譯器與環境需求

- **核心程式語言**：C++（相容 C++11 或更新標準，無任何外部第三方 C++ 套件相依）
- **建議編譯器與最佳化選項**：
  - **GCC (`g++`) / Clang (`clang++`)**：`-std=c++11 -O2 -Wall -Wextra -Wpedantic`（建議 GCC $\ge$ 9 或 Clang $\ge$ 12）
  - **MSVC (`cl.exe`, Visual Studio 2022)**：`/EHsc /O2 /W4`（MSVC v19.x x64）
- **計時器**：`std::chrono::steady_clock`（單調高解析度時鐘，不受系統時間校正影響）
- **繪圖環境 (`plot.py`)**：Python 3 + `pandas` + `matplotlib`
  ```bash
  pip install pandas matplotlib
  ```

---

## 2. 專案檔案架構與核心函式對照

| 檔案 | 說明 |
|---|---|
| `DataStructures.h` | 四種資料結構類別與節點定義、後序走訪解構子 (`~BST`, `~AVL`, `~Treap`, `~SkipList`)、可選式 AVL 逐步旋轉紀錄 (`SetVerboseRotations`) 與編譯期可關閉的記憶體追蹤器 `MemoryTracker`。 |
| `Utils.h` | 共用亂數輸入產生器 (`GenerateInsertDataset`, `GenerateQueryDataset`)、高解析度計時器 `SteadyTimer`、SplitMix64 種子混合器 `MixSeed` 與防 OOM 即時寫入 CSV 工具 (`AppendCSVRow`)。 |
| `Experiments.h` | 五組手算正確性測試 + 自訂邊界測試 + 大規模參數化驗證 (`RunCorrectnessTests`)，以及實驗一、二、三與報告輔助統計表 (`RunExperiment1`–`3`)。 |
| `main.cpp` | 指令列參數解析 (`--test`, `--exp`, `--trials`, `--seed`)。 |
| `plot.py` | 讀取 `raw_data.csv`，自動繪製並輸出符合規格之 `fig1.png`、`fig2.png`、`fig3.png`。 |
| `Makefile` | 統一輸出單一執行檔 `treebench`：預設 `make` 啟用 `MemoryTracker` 供 `--test` 檢查記憶體，`make bench` 以 `-DNO_MEMORY_TRACKER` 重新編譯 `treebench` 供正式實驗量測，並提供 `make plot`。 |

### 資料結構操作函式命名 (`DataStructures.h`)
每一種資料結構皆提供明確標示結構名稱的操作函式（單筆初始建立 `CreateXXX` 提供靜態工廠函式與 `kInitialRecord` tag 建構子，避免與 `(p, seed)` 等參數建構子產生重載歧義）：

- **`BST`**：`BST()`、`BST::CreateBST(id, score)`、`InsertBST`、`PrintBST`、`HeightBST`、`SearchAVGBST`
- **`AVL`**：`AVL(delta)`、`AVL::CreateAVL(id, score, delta)`、`InsertAVL`、`PrintAVL`、`HeightAVL`、`SearchAVGAVL`、`SetVerboseRotations(bool)`
- **`Treap` (Min-Heap)**：`Treap(seed)`、`Treap::CreateTreap(id, score, seed)`、`InsertTreap`（含外部指定 `fixed_priority` 多載）、`PrintTreap`、`HeightTreap`、`SearchAVGTreap`
- **`SkipList`（無層數上限）**：`SkipList(p, seed)`、`SkipList::CreateSkipList(id, score, p, seed)`、`InsertSkipList`（含外部指定銅板字串如 `"HHT"` 或 `std::vector<bool>` 多載）、`PrintSkipList`、`HeightSkipList`、`SearchAVGSkipList`

> **報告重點程式碼標記**：  
> `DataStructures.h` 中已用 `=== [REPORT HIGHLIGHT: ...] ===` 註解清楚標出作業要求單獨說明的三個核心區段：
> 1. **AVL Tree** 判斷高度差超過 $\Delta$ 並決定單旋轉或雙旋轉的區段 (`InsertRecAVL`)
> 2. **Treap** 將新節點依 Min-Heap 性質往上旋轉的 `while` 迴圈 (`InsertImplTreap`)
> 3. **Skip List** 擲機率 $p$ 銅板決定新節點層數的 `while` 迴圈 (`InsertImplSkipList`)

---

## 3. 編譯方式（單一執行檔 `treebench`）

本專案統一編譯為**單一執行檔 `treebench`**（Windows 為 `treebench.exe`）。為確保 `--test` 能精確驗證「零記憶體洩漏」，同時避免節點配置計數器 (`MemoryTracker`) 的遞增/遞減成本算進**實驗一的插入時間**，`Makefile` 提供兩個編譯 target 來產生同一個 `treebench` 執行檔：

1. **一般測試編譯 `make`（預設不定義 `NO_MEMORY_TRACKER` $\rightarrow$ `treebench`）**：啟用 `MemoryTracker`，供 `./treebench --test` 驗證五組手算測資、大測資不變量與後序解構子零洩漏。
2. **正式量測編譯 `make bench`（定義 `-DNO_MEMORY_TRACKER` $\rightarrow$ `treebench`）**：節點建構與解構完全不做任何計數指令，**正式執行實驗一（以及實驗二、三）量測時請使用 `make bench` 重新編譯 `treebench`**（`make exp1`、`make exp2`、`make exp3`、`make exp_all` 皆會自動先執行 `bench`）。

### Linux / macOS / WSL / MinGW (`Makefile` 或 `g++`)
```bash
# 1. 編譯測試模式 (啟用 MemoryTracker，供 --test 使用)：
make
# 或直接呼叫 g++：
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic main.cpp -o treebench

# 2. 編譯正式量測模式 (關閉 MemoryTracker，供實驗一、二、三計時使用)：
make bench
# 或直接呼叫 g++：
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic -DNO_MEMORY_TRACKER main.cpp -o treebench
```

### Windows (Visual Studio Developer Command Prompt / PowerShell)
```powershell
# 1. 測試模式 (treebench.exe，啟用 MemoryTracker)：
cl /nologo /EHsc /O2 /W4 main.cpp /Fe:treebench.exe

# 2. 正式量測模式 (treebench.exe，關閉 MemoryTracker)：
cl /nologo /EHsc /O2 /W4 /DNO_MEMORY_TRACKER main.cpp /Fe:treebench.exe
```

---

## 4. 重現正確性測試、三個實驗與三張圖的指令

> Windows PowerShell 請將 `./treebench` 改為 `.\treebench.exe`（參數亦支援單破折號別名 `-exp`、`-trials`、`-seed`）。

### (1) 正確性測試
```bash
make test
# 或：
./treebench --test
```
包含 7 組驗證測試與記憶體檢查：
- **Test 1–5**：作業 PDF 第 4 頁規定的五組手算小輸入（其中 **Test 2** 啟用 `AVL::SetVerboseRotations(true)`，逐步印出「第 3 次插入 `(3, 70)` 在 `id=1` 做 `RR` 旋轉、第 5 次插入 `(5, 70)` 在 `id=3` 做 `RR` 旋轉」）。
- **Test 6**：自訂邊界測試（同時檢驗 `AVL` 的 `LR`/`RL` 雙旋轉與重複 ID 更新、`Treap` 重複 ID 優先度陷阱、以及 `SkipList` 超過 32 層之無上限擴展）。
- **Test 7**：大規模參數化驗證：
  - **7A (`AVL` $\Delta \in \{2, 3, 5\}$)**：各以 20,000 筆隨機輸入與 20,000 筆遞增輸入 (`id = 1..20000`) 插入後驗證 `VerifyAVL() == true`（抓出將平衡條件寫死為 `1`、未更新高度或單/雙旋轉判斷在 $\Delta > 1$ 時失效的錯誤）。
  - **7B (`SkipList` $p \in \{1/4, 3/4\}$)**：各插入 50,000 筆資料後驗證 `VerifySkipList() == true`（抓出將銅板機率寫死為 $1/2$、設定 32 層上限截斷高塔或跨層指標串接失序的錯誤）。

### (2) 圖 1 數據 — 實驗一：插入 $n$ 筆資料的平均總時間 ($n = 2^0 \dots 2^{20}$)
> **注意**：正式量測請使用 `make bench` 編譯出的 `./treebench`（定義 `-DNO_MEMORY_TRACKER`），確保節點建構/解構不含任何 `MemoryTracker` 計數成本。
```bash
make exp1
# 或：
./treebench --exp 1 --trials 10 --seed 0
```
- **比較對象**：`BST`、`AVL(delta=1)`、`Treap`、`SkipList(p=1/2)`
- **短區間校準**：當單次建構時間 $< 1\text{ ms}$ 時，自動以同一份輸入建立 $r$ 個獨立結構一起計時再除以 $r$，並於停止計時後才釋放全部 $r$ 個結構。
- **輸出檔案**：`exp1_insert_time.csv` 以及整合原始數據檔 `raw_data.csv`。

### (3) 圖 2 數據 — 實驗二：一次 `SearchAVG` 的平均搜尋時間 ($n = 2^0 \dots 2^{20}, Q = 100,000$)
```bash
make exp2
# 或：
./treebench --exp 2 --trials 10 --seed 0
```
- **比較對象**：`BST`、`AVL(delta=1)`、`Treap`、`SkipList(p=1/2)`
- **防最佳化與命中率統計**：將 $Q$ 次查詢回傳值加總並印出以防止編譯器 `-O2` 刪除迴圈；計時結束後重跑同一組 $Q$ 筆查詢，統計未回傳 `-1`（查得到）的比例寫入 `exp2_hit_ratio.csv`（供報告實驗二第 2(a) 題使用）。
- **輸出檔案**：`exp2_search_time.csv`、`exp2_hit_ratio.csv` 以及 `raw_data.csv`。

### (4) 圖 3 數據 — 實驗三：七個資料結構的平均高度 ($n = 2^0 \dots 2^{20}$) 與報告輔助統計表
```bash
make exp3
# 或：
./treebench --exp 3 --trials 10 --seed 0
```
- **比較對象**：`BST`、`AVL(delta=1)`、`AVL(delta=5)`、`Treap`、`SkipList(p=1/2)`、`SkipList(p=3/4)`、`SkipList(p=1/4)`
- **附加分析輸出 (`exp3_report_tables.txt`)**：
  1. $n = 2^{10} \dots 2^{20}$ 區間七條線的實測斜率 $\Delta h / \Delta \log_2 n$（第 1 題）
  2. $n = 2^{10}$ 遞增輸入 `id = 1..1024` 下七個結構的高度表（第 2 題）
  3. $\Delta = 1$ 與 $\Delta = 5$ 在 $2^{20}$ 節點的理論最高高度 $S_h$ 遞迴計算與實測平均旋轉次數（雙旋轉計為 2 次，第 3 題）
  4. 三種 $p \in \{1/4, 1/2, 3/4\}$ 在 $n = 2^{20}$ 時的平均高度與平均搜尋步數（往右一步 +1、往下一層 +1，獨立於實驗二計時函式外，第 4 題）
- **輸出檔案**：`exp3_height.csv`、`exp3_report_tables.txt` 以及 `raw_data.csv`。

### (5) 一次依序跑完三個實驗
```bash
make exp_all
# 或：
./treebench --exp all --trials 10 --seed 0
```

### (6) 使用 `plot.py` 繪製圖 1、圖 2、圖 3
當跑完三個實驗並產生 `raw_data.csv` 後，執行以下指令即可自動繪製並輸出 `fig1.png`、`fig2.png`、`fig3.png`：
```bash
make plot
# 或直接呼叫 Python：
python3 plot.py
# (Windows: python plot.py)
```
- **共同橫軸設定**：插入資料筆數 $n$，採 $\log_2$ 對數刻度 (`base=2`)，刻度標籤完整標示 $2^0, 2^1, \dots, 2^{20}$。
- **`fig1.png`（實驗一，4 條線）**：縱軸為**對數刻度**，單位為 `ms`（平均總插入時間）；圖例標明 `BST`、`AVL tree ($\Delta = 1$)`、`Treap (Min-Heap)`、`Skip list ($p = 1/2$)`。
- **`fig2.png`（實驗二，4 條線）**：縱軸為**線性刻度**，單位為 `ns`（單次 `SearchAVG` 平均搜尋時間）；圖例同上。
- **`fig3.png`（實驗三，7 條線）**：縱軸為**線性刻度**，單位為 `層`（平均高度）；圖例完整標明 7 組結構及其參數 $\Delta \in \{1, 5\}$ 與 $p \in \{1/2, 3/4, 1/4\}$。

---

## 5. 隨機種子設計與可重現性說明

作業規範要求（PDF 第 3 頁）：
- 每個設定產生至少 10 份不同的輸入，每份用不同的隨機種子，圖上畫這 10 次的平均。
- 指令列給一個起始種子，第 $i$ 份輸入用「起始種子加 $i$」或明確寫在 `README.md` 的做法。
- `Treap` 的 `priority` 與 `SkipList` 的銅板也要由種子決定；同一個種子跑兩次，圖 3 的高度要一模一樣。

### (1) 預設與建議起始種子
- 指令列參數：`--seed <S>`（預設值為 **`0`**，即 `./treebench --exp all --trials 10 --seed 0`）。
- 亂數引擎：統一使用 C++11 標準 64 位元梅森旋轉演算法 `std::mt19937_64`。

### (2) 第 $i$ 個 Trial 的種子衍生公式 (`DeriveTrialSeeds` in `Experiments.h`)
給定指令列起始種子 `base_seed` ($S$)、資料量指數 $k = \log_2 n \in \{0, 1, \dots, 20\}$，以及第 $i$ 次重複實驗（$i \in \{0, 1, \dots, \text{trials}-1\}$）：

1. **起始種子加 $i$**：
   $$\text{s} = \text{base\_seed} + i$$
2. **結合指數 $k$ 並以 SplitMix64 雙射雜湊展開 (`MixSeed` in `Utils.h`)**：  
   為了讓不同 $n = 2^k$ 之間的亂數序列彼此獨立、不發生前綴重疊，先計算：
   $$\text{key} = \text{MixSeed}\big(\text{s} \oplus (k \ll 32)\big)$$
   接著衍生出三個互不干擾的 64-bit 子種子：
   ```cpp
   insert_seed = MixSeed(key ^ 0x1111111111111111ULL); // 產生 n 筆 (id, score) 輸入
   query_seed  = MixSeed(key ^ 0x2222222222222222ULL); // 產生 Q = 100,000 筆查詢 id
   ds_seed     = MixSeed(key ^ 0x3333333333333333ULL); // Treap priority 與 SkipList 擲銅板
   ```

| 子種子名稱 | 用途說明 |
|---|---|
| `insert_seed` | 傳入 `GenerateInsertDataset`，從 `UNI(1, 2^20)` 與 `UNI(0, 100)` 抽樣產生 $n$ 筆 `(id, score)`。**同一個 `(k, i)` 中，所有資料結構皆拿到完全相同的這一份輸入。** |
| `query_seed` | 傳入 `GenerateQueryDataset`，從 `UNI(1, 2^20)` 抽樣產生 $Q = 100,000$ 個查詢 `id`。**同一個 `(k, i)` 中，四種資料結構在實驗二皆查詢完全相同的這一組 ID。** |
| `ds_seed` | 傳入 `Treap(ds_seed)`（決定 $(0,1)$ 均勻隨機 `priority`）與 `SkipList(p, ds_seed)`（決定每次插入的擲銅板結果）。在實驗一 $n$ 很小需建立 $r$ 個獨立結構時，第 `rep` 個結構使用 `MixSeed(ds_seed + rep)`。 |

### (3) 可重現性保證
- **跨結構公平比較**：在同一個 `(n, trial i)` 內，所有受測資料結構面對完全相同的插入序列與查詢序列。
- **跨實驗一致性**：實驗一、實驗二與實驗三共用同一個 `DeriveTrialSeeds(base_seed, k, i)`，因此無論分開執行 `--exp 1`、`--exp 2`、`--exp 3` 或一次執行 `--exp all`，建出的樹與跳躍串列結構皆完全一致。
- **百分百可重現**：使用相同 `--seed`（如 `--seed 0`）重複執行兩次，實驗三（圖 3）量到的所有平均高度、旋轉次數、搜尋步數與實驗二的查詢回傳值總和皆**完全相同**。

---

## 6. 其他關鍵實作設計說明

1. **重複 ID 處理（每個 ID 單一節點）**  
   四種資料結構統一採用「每個相異 `id` 只建立一個節點」，節點內維護 `long long sum_score` 與 `int count`。插入已存在的 `id` 時直接原地累加總分與次數，不新增重複節點、亦不觸發額外旋轉；搜尋命中時在第一個符合的節點即可立即回傳 `(double)sum_score / count`，查無此 `id` 則回傳 `-1`。

2. **記憶體管理與防 OOM 資料保護**  
   - 每個 trial 結束時，資料結構皆透過嚴格的**後序走訪解構子 (Post-order Traversal Destructor)** 立即釋放所有動態配置節點，避免跨 trial 累積記憶體。
   - 每完成一個 $n$ 的 10 次平均，立即以 `std::ios::app` 寫入並 `flush` 至 CSV 檔（`raw_data.csv` 包含 `exp, log2_n, n, structure, unit, mean_value, trials` 欄位，可直接由 `plot.py` 讀取繪製圖 1～圖 3）。
