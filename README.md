# Image-Editor 使用者手冊

迷你 Photoshop：讀取 Targa（.tga）圖檔，執行色彩、抖動、濾波、縮放等操作，並存檔。所有操作都可透過腳本語言批次執行。

## 1. 建置執行檔

### 1.1 VS IDE（最簡單）

1. 以資料夾方式開啟 repo 根目錄，VS 會自動用 `CMakeLists.txt` configure。
2. 組態選 `x64-Debug` → 建置 → 建置全部。
3. 執行檔位於 `out/build/x64-Debug/ImageEditing.exe`。

### 1.2 命令列

不可直接調 VS 自帶的 cmake/ninja（會缺 MSVC 環境變數，連 `<functional>` 都找不到），要先載 `vcvars64.bat`：

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >NUL && C:\PROGRA~1\MIB055~1\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe -C C:\Users\egg20\Documents\GitHub\Image-Editor\out\build\x64-Debug'
```

注意：

- 重連前先關掉跑中的程式，否則連結報 `LNK1168`：`taskkill /F /IM ImageEditing.exe`
- configure 已存在時只需上面一行；改完 `src/TargaImage.cpp` 存檔後直接重跑即可。

## 2. 兩種使用方式

### 2.1 GUI 模式

不加參數直接跑 `ImageEditing.exe`，會開一個圖片視窗加單行指令框：

- 逐行輸入指令（不用引號），Enter 執行；空 Enter 重跑上一條。
- 例：`load ../../../Images/wiz.tga` → `gray` → `save ../../../out_gray.tga`
- `load` 進來的圖會直接顯示在視窗裡（含 `diff` 完的比對圖），本程式自己就是 tga 看圖軟體。

### 2.2 Headless 腳本模式

```powershell
ImageEditing.exe -headless script.txt
```

`script.txt` 格式：一行一個指令，檔尾要有換行。例（`bright_test.scr`）：

```
load C:/Users/egg20/Documents/GitHub/Image-Editor/Images/wiz.tga
dither-bright
save C:/Users/egg20/Documents/GitHub/Image-Editor/Images/my_bright.tga
```

路徑是相對於「行程工作目錄」的相對路徑。exe 在 `out/build/x64-Debug/` 時，`Images/wiz.tga` 會找不到，要嘛寫絕對路徑，要嘛先 `cd` 到 repo 根目錄再跑，要嘛用 `../../../Images/wiz.tga`。

## 3. 指令一覽（共 29 個可用）

### 3.1 檔案／流程

| 指令 | 參數 | 說明 |
|---|---|---|
| `load` | `<檔名>` | 載入圖檔成為 current image（Audrey 起點） |
| `save` | `<檔名>` | 存 current image（只寫 tga） |
| `run` | `<腳本>` | 執行另一個腳本檔 |
| `diff` | `<檔名>` | `current = \|current − 指定檔\|`，結果存回 current |

### 3.2 灰階／量化／抖動

| 指令 | 說明 |
|---|---|
| `gray` | `I = 0.299r + 0.587g + 0.114b`，alpha 不動 |
| `quant-unif` | 均勻量化 8×8×4 = 256 色（R/G `{0,36,73,109,146,182,219,255}`，B `{0,85,170,255}`） |
| `quant-pop` | 先 32 階直方圖（32768 格），取前 256 熱門色，L2 最近色映射 |
| `dither-thresh` | 門檻 0.5 二值化 |
| `dither-bright` | 分位數門檻，輸出平均亮度嚴格等於輸入 |
| `dither-rand` | 強度 0~1 加 `[-0.2, 0.2]` 均勻雜訊，門檻 0.5（固定種子，可重現） |
| `dither-cluster` | 4×4 聚集點 tile（n/17 精確值，`tile[y%4][x%4]`） |
| `dither-fs` | Floyd-Steinberg 誤差擴散＋蛇行掃描 |
| `dither-color` | 彩色 Floyd-Steinberg，調色盤同 `quant-unif` |

### 3.3 濾波／風格／尺寸／合成

| 指令 | 參數 | 說明 |
|---|---|---|
| `filter-box` | — | 5×5 box filter |
| `filter-bartlett` | — | 5×5 Bartlett filter |
| `filter-gauss` | — | 5×5 Gaussian filter |
| `filter-gauss-n` | `<奇數N>` | N×N 二項式 Gaussian（N 必須是奇數） |
| `filter-edge` | — | 邊緣偵測 |
| `filter-enhance` | — | 邊緣增強 |
| `npr-paint` | — | Hertzmann painterly rendering 簡化版 |
| `half` | — | 1/2 縮小（Bartlett 重建） |
| `double` | — | 2 倍放大（Bartlett 重建） |
| `scale` | `<倍率>` | 等比縮放（須 > 0） |
| `rotate` | `<角度>` | 順時針旋轉（度，不可為 0，同尺寸輸出，空缺補黑） |
| `comp-over/in/out/atop/xor` | `<檔名>` | 與指定圖做 alpha 合成 |

> `dither-pattern` 只有關鍵字、沒有實作分支，打了只會 `Unable to parse command`，不要用。

## 4. 驗證流程（拿輸出跟參考答案比）

### 4.1 `diff` 比對（程式內建）

比的是「同操作對同操作」，不是跟原圖比：

```
load out_my_gray.tga
diff ref_gray.tga
save diff_check.tga
```

全黑 = 一致。各操作的允收標準不同：

- 要求近全黑：`gray`、`dither-thresh/bright/cluster`（只允許黑白交界零星像素）、`filter-*`（只允許邊界一圈）、`dither-fs`（只要求神韻像）
- 一定有殘影、屬正常：`quant-unif/pop`（看 banding 風格）、`dither-rand`、`npr-paint`、`half/double/scale/rotate`（允許邊界差）

### 4.2 ImageMagick 輔助（建議安裝，驗收快很多）

```powershell
magick identify -format "%k" out.tga        # 相異色數：黑白抖動應=2，量化應≤256
magick out.tga -format %c histogram:info: | Sort-Object { [int](($_ -split ':')[0]) } -Descending | Select-Object -First 20
magick out.tga -alpha off -format "%[fx:mean]" info:   # 純 RGB 平均（不加 -alpha off 的話全黑不透明圖是 0.25 不是 0）
magick compare -metric RMSE my.tga ref.tga NUL:        # 跨格式也可比（.tga vs .png）
magick compare -metric AE -fuzz 0 my.tga ref.tga NUL:  # 直接數差異像素
magick out.tga out.png                                  # tga 轉 png，用相片/瀏覽器看
```

## 5. 常見問題

1. **`Unable to load image: Images/wiz.tga`**：工作目錄問題（§2.2），改用絕對路徑或 `../../../Images/wiz.tga`。
2. **永遠印 `TGA Error: %s`**：舊 code 的 `cout` bug（逗號運算子），印不出真正原因，無視它，先查路徑。
3. **建置報 `LNK1168`**：舊 exe 還在跑，先 `taskkill /F /IM ImageEditing.exe` 再連。
4. **建置報 `functional找不到`**：沒載 MSVC 環境，用 §1.2 的 `vcvars64.bat` 寫法。
5. **`filter-gauss-n` 報錯**：N 必須是奇數；`scale` 須 >0；`rotate 0` 會被判 invalid。
6. **`Images/` 下的新檔案 git 看不到**：`.gitignore` 的 `/Images` 規則故意的（測試輸出保持 local）；已追蹤的 20 張原圖照樣推送。`lib/Debug`、`lib/Release` 已用 `!` 例外救回，14 個 `.lib` 追蹤正常。
