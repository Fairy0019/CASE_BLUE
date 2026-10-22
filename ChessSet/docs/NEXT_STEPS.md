# ChessSet 3D 国际象棋 · 下一步计划

| 项目 | 内容 |
| --- | --- |
| 基线 | `main` @ `9c03dd7`（2026-10-22，Add Godot 3D chess game） |
| 对象 | 仓库内最新的 3D 项目 `ChessSet/` |
| 文档分支 | `docs/chessset-next-plan` |
| 编写日期 | 2026-10-22 |
| 目标 | 把当前“可玩的 3D Demo”推进到“规则可信、构建可复现、体验完整、可持续迭代”的 v1.x 产品 |

> 仓库里此前的 `2026.7.8/`、`C语言（样本）/` 是控制台/2D 国际象棋样本；`9c03dd7` 新增的 `ChessSet/` 是最新的 3D 项目，因此本计划以它为准。

## 1. 结论（TL;DR）

当前 ChessSet 已经具备一个 3D 国际象棋游戏的核心骨架：完整走子规则、人机/双人、悔棋、升变、棋谱、音效、Windows 发布包。下一阶段不建议立刻加联网或重做美术，而应按以下顺序推进：

1. **先稳定**：修复规则边界和人机悔棋死锁，补齐自动化测试与 CI，让“新克隆的仓库”不依赖本机 Blender 配置也能导入、测试、构建。
2. **再补完**：棋钟、认输/提和、PGN/FEN、设置、存档、异步 AI、响应式 UI，把“能下完一局”变成“像一款完整棋类产品”。
3. **后扩展**：跨平台发布、视觉/音频打磨、国际化、无障碍，最后再评估联网对战、分析模式和棋题。

建议版本节奏：

| 阶段 | 版本 | 周期 | 主题 | 退出条件 |
| --- | --- | --- | --- | --- |
| Phase 0 | v1.0.1 | 1 周 | 稳定化、可复现 | 100% 测试通过，fresh clone 可 headless 导入/构建 |
| Phase 1 | v1.1.0 | 2 周 | 完整对局体验 | 棋钟、PGN/FEN、认输/提和、设置、存档可用 |
| Phase 2 | v1.2.0 | 3 周 | 产品化与跨平台 | Windows/Linux/Web 构建通过，UI 在 1280×720 起可用 |
| Phase 3 | v2.0.0 | 4–6 周 | 在线与内容（可选） | 联机对局、分析/棋题至少一项上线 |

Phase 3 必须在前两阶段验收通过后再启动，避免“规则还没稳，先做联网”的典型范围失控。

---

## 2. 现状盘点

### 2.1 已实现能力

- 3D 棋盘与 32 个独立棋子，真实比例（512 mm 棋盘），程序化木纹、SSAO、软阴影、桌面渐变。
- 王、后、车、象、马、兵完整走子；王车易位、吃过路兵、升变。
- 将军、将死、逼和、50 回合、子力不足和棋。
- 人机（Alpha-Beta，深度 3）与本地双人；可选执白/执黑；AI 执黑时自动先手。
- 合法落点高亮、吃子/将军/将死提示、升变对话框、悔棋、新局、视角翻转、鼠标环绕/缩放。
- 棋谱面板、提子统计、回合指示、结算界面。
- 程序化音效、自定义启动画面、Windows 导出预设与 v1.0.0 Release。

### 2.2 代码与资产结构

| 文件 | 规模 | 职责 |
| --- | --- | --- |
| `ChessSet/scripts/main.gd` | 约 1731 行 | 场景、相机、材质、UI、点击、动画、音效、对局流程全部集中 |
| `ChessSet/scripts/chess_game.gd` | 约 703 行 | 规则引擎、FEN、走子/悔棋、perft |
| `ChessSet/scripts/chess_ai.gd` | 约 213 行 | Alpha-Beta、子力/位置表评估、着法排序 |
| `ChessSet/ChessSet.blend` | 约 7.2 MB | 原始 Blender 模型，由 Godot 直接导入 |
| `ChessSet/scenes/main.tscn` | 仅根节点 | 运行时构建 3D/UI，场景本身几乎是空的 |
| `ChessSet/assets/` | 音效、shader、启动图 | 程序化/自制素材 |

### 2.3 实测基线

在 Godot 4.7.2 headless 下直接调用规则引擎与 AI，得到以下基线（仅代表当前测试机）：

| 项目 | 结果 |
| --- | --- |
| 起始局面 perft(1/2/3) | `20 / 400 / 8902`，正确 |
| Kiwipete perft(1/2) | `48 / 2039`，正确 |
| AI 深度 3（起始局面） | 约 179 ms，897 节点 |
| AI 深度 3（Kiwipete） | 约 541 ms，2,575 节点 |
| AI 深度 4（Kiwipete） | 约 5.76 s，7,140 节点 |
| `godot --headless --editor --quit` | 失败：`Blender path is invalid or not set... Cannot configure blender path in headless mode` |

结论：规则引擎的 perft 基础是对的，但**没有把这些验证固化成测试**；AI 深度 3 尚可，深度 4 已会明显阻塞主线程；`.blend` 直接导入在 CI/无编辑器配置环境下不可用。

---

## 3. 主要问题与优先级

### P0：发布前必须解决

#### P0-1 人机模式执黑悔棋会卡死

`main.gd::_undo()` 在人机模式下固定撤销 2 步。执黑时流程是：

1. AI 先走一手（history = 1）；
2. 玩家走一手（history = 2）；
3. 玩家悔棋，撤销 2 步，回到开局；
4. 此时 `turn == WHITE`、`mode == MODE_AI`、`player_side == BLACK`，但 `_undo()` 没有重新调用 `_start_ai_turn()`；
5. 界面停在“AI 回合”，玩家无法落子，AI 也不会再走。

**修复**：悔棋/新局/读档后统一走一个 `_resume_turn()`：若 `mode == AI && game.turn != player_side && !game.game_over`，则启动 AI；同时保留 `session_id` 防止旧协程落子。补一条回归测试。

#### P0-2 和棋判定不完整

- 没有三次重复、五次重复、75 回合自动和棋。
- `chess_game.gd::is_insufficient_material()` 只判断“minor ≤ 1”：K+B vs K+B（同色格象）这种死局会漏判；反过来也没有区分 K+N vs K+N 等“不能强制将死但仍有将死可能”的局面。
- `_update_status()` 先检查 50 回合再检查将死；如果最后一手正好将死且 halfmove 到 100，应按“将死优先”处理。
- 没有提和/认输，50 回合也只有自动判和，没有“可申请”的语义。

**修复**：新增 `is_dead_position()`、Zobrist/局面计数、`can_claim_threefold()`、`can_claim_fifty_move()`；状态机按“将死/逼和 → 自动和棋 → 可申请和棋”排序。用 FEN 用例覆盖同色象、异色象、K+N vs K+N、K+B vs K+N 等边界。

#### P0-3 没有自动化测试与 CI

README 声称用 perft 验证过，但仓库中没有 `tests/`、没有测试运行器、没有 `.github/workflows/`。规则引擎是纯 `RefCounted`，非常适合 headless 测试，应优先补上：

- perft：起始局面、Kiwipete、Position 3/4/5/6；
- FEN 往返、走子/悔棋不变量、升变、易位、吃过路兵；
- 将死/逼和/50 回合/三次重复/死局；
- 随机对局 fuzz：随机走合法着法 N 千步，检查不出现非法状态、不崩溃；
- AI 冒烟：固定种子下能在时限内返回合法着法。

建议用 GdUnit4 或一个极简 `SceneTree` 测试入口，保证 `godot --headless --script tests/run_tests.gd` 退出码为 0。

#### P0-4 构建不可复现

- `ChessSet.blend` 由 Godot 的 Blender 导入器直接读取；新环境/CI 没有配置 Blender 路径时导入失败（已实测）。
- `export_presets.cfg` 里写死了 `D:/SteamLibrary/.../4.7.2.stable/...` 模板路径。
- README 使用 `D:\ChessSet\project.godot` 这类本机路径；新克隆者无法照做。
- 没有锁定 Godot/Blender 版本，也没有构建脚本。

**修复**：把模型导出为 `ChessSet.glb`（保留 `.blend` 作为源文件），Godot/CI 只依赖 glb；删除导出预设中的绝对模板路径，补 `tools/export_model.sh`、`tools/build.sh`；在 README 写清版本与命令。目标是：**fresh clone → `godot --headless --import` → 测试 → 导出，全程不需要手工配置 Blender。**

#### P0-5 1280×800 下棋盘被裁切

现有 `screenshots/demo.png`、`selected.png` 中棋盘左下角/A 线被屏幕边缘裁掉；右侧面板固定 308 px，相机参数固定 `BASE_DIST = 0.86`、`fov = 46`，没有根据视口和面板宽度计算安全取景。

**修复**：根据可用 3D 视口（扣除右侧面板）计算相机距离/目标偏移，至少覆盖 1280×720、1280×800、1920×1080、16:10/21:9；加截图对比或人工验收清单。

#### P0-6 仓库发布卫生

- 没有 `LICENSE`（代码、模型、音效的授权需要明确）。
- 没有 `CHANGELOG.md`、`CONTRIBUTING.md`、issue/PR 模板。
- 根目录 `蓝色计划-策划书.txt` 是 0 字节空文件。
- `ChessSet/node_2d.tscn`、`node_2d_2.tscn` 是遗留空场景。
- README 里仍有 `D:\建模blender\#0001` 等本机路径。

这些不阻塞玩法，但会直接影响别人能否合法使用、复现和贡献。

### P1：下一版本应补完

| 问题 | 影响 | 建议 |
| --- | --- | --- |
| AI 同步执行、固定深度 3 | 深度 4 约 5.8 s，搜索期间 UI 冻结 | 迭代加深 + 时间预算 + 独立线程；保留主线程只落子 |
| 无棋钟 | 无法进行正式对局 | 突然死亡/加秒、暂停、超时判负 |
| 无 PGN/FEN 导入导出 | 无法复盘、分享、接分析工具 | SAN 记谱、PGN 读写、FEN 复制/粘贴 |
| 无认输/提和/再战 | 对局闭环不完整 | 结算面板增加“认输/提和/交换再战” |
| 无设置/存档 | 每次启动都从零开始 | AI 难度、音量、动画速度、合法点显示、语言；`user://` 自动存档 |
| `main.gd` 1731 行、运行时建 UI | 难测试、难扩展、难做响应式 | 拆成 `GameController`、`BoardView`、`HUD`、`SettingsPanel`、`AudioManager` |
| 棋谱是长代数记法 | 不是标准 SAN，PGN 不可用 | 加消歧、吃子、将军/将死后缀；用标准 PGN 用例验证 |
| 系统字体、硬编码中文 | Linux/macOS/Web 字体不一致 | 内置 Noto Sans SC 子集 + 字体授权；i18n（zh/en） |
| 单击音效只有一个播放器 | 快速走子会截断上一声 | AudioStreamPolyphonic 或播放器池 + 音量总线 |
| 无触摸/小屏适配 | 移动端/Web 不可用 | 触摸拖拽、点选、双指缩放；最小 1280×720 布局 |

### P2：增长与平台化

- 联网对战（房间/匹配、断线重连、服务端权威时钟）。
- 分析模式（引擎线、评分、失误标记）、棋题/教程。
- 跨平台发布（Linux、macOS、Web、itch.io/Steam）。
- 无障碍（色盲配色、高对比、键盘全导航、屏幕阅读器标签）。
- 成就/Elo/统计、可选的匿名崩溃报告。
- 视觉/音频进一步打磨（PBR 木纹、反射、环境音、棋子动画曲线）。

---

## 4. 路线图与任务清单

### Phase 0：稳定化（第 1 周，v1.0.1）

**目标**：规则边界可信、测试可跑、构建可复现、UI 不裁切。

| ID | 任务 | 具体改动 | 验收 |
| --- | --- | --- | --- |
| P0-01 | 修人机悔棋死锁 | `_undo()` 后调用统一的 `_resume_turn()`；加 `session_id` 保护 | 执黑悔棋后 AI 自动走；连续 20 局不卡死 |
| P0-02 | 补死局判定 | `is_dead_position()`：K vs K、K+B/K+N vs K、K+B vs K+B 同色；不误判 K+N vs K+N | 对应 FEN 用例全部通过 |
| P0-03 | 三次重复/75 回合 | Zobrist 或 FEN 计数；三次可申请、五次自动、75 回合自动 | 构造局面测试通过；AI 不再主动重复 |
| P0-04 | 测试框架 | `tests/` + perft/FEN/undo/fuzz/AI 冒烟 | `godot --headless --script tests/run_tests.gd` 退出 0 |
| P0-05 | CI | GitHub Actions：导入、测试、导出 Windows artifact | PR 上绿灯；artifact 可下载运行 |
| P0-06 | 模型构建链 | 导出 `ChessSet.glb`；删除 `D:/...` 模板路径；`tools/build.sh` | fresh clone 无 Blender 也能 `--import` |
| P0-07 | 相机/布局 | 按视口计算相机距离与目标偏移；右侧面板自适应 | 1280×720/800、1920×1080 棋盘完整可见 |
| P0-08 | 仓库卫生 | LICENSE、CHANGELOG、根 README、删除空文件/空场景、修 `D:\` 路径 | 文档审查通过 |

### Phase 1：完整对局（第 2–3 周，v1.1.0）

**目标**：一局棋从开局到结算、复盘、再战都闭环。

| ID | 任务 | 验收 |
| --- | --- | --- |
| P1-01 | 棋钟（突然死亡 + 加秒、暂停、超时） | 双方时间独立；超时判负；重开重置 |
| P1-02 | 认输/提和/再战 | 结算面板可操作；AI 可接受/拒绝提和（简单策略） |
| P1-03 | PGN/FEN | 导出标准 SAN PGN；导入 PGN/FEN 可继续下；非法输入有提示 |
| P1-04 | 棋谱交互 | 点击棋谱跳到历史局面；前进/后退；当前着高亮 |
| P1-05 | 设置与存档 | AI 难度、音量、动画速度、合法点、语言；退出自动存档，启动恢复 |
| P1-06 | AI 异步与分级 | 迭代加深 + quiescence + 时间预算；入门/休闲/进阶/挑战；搜索不阻塞 UI |
| P1-07 | UI 拆分 | `main.gd` 降到流程控制；Board/HUD/Settings/Audio 独立场景或脚本；信号解耦 |
| P1-08 | 响应式与触摸 | 1280×720 起不重叠；触摸可完成选子/落子/旋转/缩放 |
| P1-09 | 字体与 i18n | 内置中文字体；zh/en 切换；无系统字体也能正常显示 |
| P1-10 | 音频总线 | 主音量/音效音量；快速连续走子不截断 |

### Phase 2：产品化（第 4–6 周，v1.2.0）

**目标**：可对外发布的多平台版本。

| ID | 任务 | 验收 |
| --- | --- | --- |
| P2-01 | 视觉打磨 | 相机预设、木纹/PBR、反射/环境光、质量档位；截图评审通过 |
| P2-02 | 音频打磨 | 环境音/音乐（可关）、胜负音效层次、淡入淡出 |
| P2-03 | 跨平台导出 | Windows/Linux/Web 预设与 CI artifact；Web 首屏可玩 |
| P2-04 | 性能 | 1080p 60 FPS（集显）、加载 < 3 s、安装包目标 < 80 MB |
| P2-05 | 无障碍 | 色盲配色、高对比、键盘全导航、焦点可见 |
| P2-06 | 发布流程 | 版本号、CHANGELOG、签名/校验和、itch.io/Release 页面 |
| P2-07 | 稳定性 | 100 局 AI 自对弈无崩溃/非法着法；崩溃日志可选上报 |

### Phase 3：在线与内容（第 7–12 周，v2.0.0，可选）

- 联网对战：房间码/匹配、服务端权威棋钟、断线重连、观战。
- 分析模式：引擎评分、最佳线、失误高亮。
- 棋题/教程：残局/战术题库、进度。
- 成就/Elo/统计。
- 先做 1 个服务器可承载的小规模测试，再决定是否公开匹配。

**明确不做**：Phase 0/1 期间不引入联网、不做引擎重写、不换美术管线；避免把稳定性问题拖到联机阶段。

---

## 5. 技术方案要点

### 5.1 规则正确性

建议把“局面状态”收敛到一个明确的状态机：

```text
走子
  -> 更新 board/turn/castling/ep/halfmove/fullmove
  -> 若对方无合法着法：将死 / 逼和（最高优先级）
  -> 若 75 回合 / 五次重复 / 死局：自动和棋
  -> 若 50 回合 / 三次重复：标记“可申请和棋”
  -> 否则继续
```

实现细节：

- `position_key()`：Zobrist 或 `board + turn + castling + ep` 的稳定哈希；`position_counts` 只在 `make_move/undo` 维护，搜索走子不计数。
- `is_dead_position()`：
  - K vs K；
  - K+B vs K、K+N vs K；
  - K+B vs K+B 且双方象在同色格；
  - 其余“不能强制将死但仍有将死可能”的局面不判死局（如 K+N vs K+N、K+NN vs K）。
- `to_fen()` 的 ep 字段只在“确实存在合法吃过路兵”时输出，避免局面哈希/重复判定误判。
- `make_move()` 增加合法性断言（UI 之外也能安全调用），搜索使用单独的 `make_search_move()`。

### 5.2 AI

当前 `ChessAI.find_best_move()` 是固定深度、同步、无静态搜索。建议：

1. **迭代加深 + 时间预算**：`depth 1..N`，每层检查 `Time.get_ticks_msec()`；超时返回上一层最佳着法。
2. **Quiescence**：只扩展吃子/升变/将军，消除明显的 horizon effect。
3. **置换表**：Zobrist key + depth/score/flag/best move；注意重复局面路径相关分数。
4. **着法排序**：TT best、吃子 MVV-LVA、升变、killer/history；现有 `_order_moves` 可保留为第一版。
5. **异步**：`Thread` 或 `WorkerThreadPool` 跑搜索，主线程轮询/回调；`session_id` 失效时丢弃结果。
6. **难度**：入门（depth 1–2 + 随机失误）、休闲（~1.2 s）、进阶（~2.5 s）、挑战（~5 s）；不要只暴露“深度”给玩家。
7. **测试**：固定种子、固定 FEN、固定时间预算，断言返回合法着法；不要求固定具体着法。

### 5.3 架构拆分

`main.gd` 目前同时承担场景、渲染、UI、输入、动画、音效、对局流程。建议按职责拆：

```text
scenes/main.tscn
  Main (GameController)
    BoardView        # 棋盘/棋子/高亮/相机/点击
    HUD              # 回合、棋谱、按钮、结算
    SettingsPanel
    AudioManager
    ChessGame        # 纯规则，RefCounted
    ChessAI
```

- `ChessGame` 保持无 Node 依赖，便于 headless 测试。
- `GameController` 只处理“选中→走子→AI→结算”的流程和信号。
- UI 用 `.tscn` 搭骨架，脚本只做绑定/更新；避免 1700 行运行时建 UI。
- 先拆 BoardView/HUD 两块即可，不必一次重构完。

### 5.4 构建、测试与 CI

建议的仓库结构：

```text
ChessSet/
  tests/
    run_tests.gd
    perft_cases.gd
    rules_cases.gd
  tools/
    export_model.sh
    build.sh
  .github/workflows/ci.yml
  docs/
  ChessSet.glb          # 由 ChessSet.blend 导出，CI 直接使用
```

CI 步骤：

1. 安装/缓存 Godot 4.7.2；
2. `godot --headless --path ChessSet --import`；
3. `godot --headless --path ChessSet --script tests/run_tests.gd`；
4. 导出 Windows/Linux（后续加 Web）；
5. 上传 artifact；打 tag 时创建 Release。

注意：如果继续保留 `.blend` 直接导入，CI 必须安装 Blender 并写入编辑器设置；更稳妥的是提交导出的 `.glb`，把 `.blend` 作为源文件。

### 5.5 UX / 视觉

- **相机**：用可用视口宽高比计算距离，保证棋盘四角在安全区内；右侧面板宽度变化时重新计算；提供“重置视角”与 2–3 个预设机位。
- **布局**：1280×720 为最低支持分辨率；小于该尺寸时侧栏改为抽屉/底部面板。
- **字体**：内置 Noto Sans SC（OFL）与一款标题字体；避免 `SystemFont` 在 Linux/Web 上回退。
- **反馈**：走子/吃子/将军的动画时长可配置；AI 思考时显示进度/可取消；非法点击有轻提示。
- **可访问性**：颜色不是唯一信息（合法点同时有形状/图标）；键盘 Tab/Enter/Esc 可完成主要操作。

---

## 6. 验收指标

### 规则与 AI

- perft 基准全部精确匹配（起始、Kiwipete、Position 3/4/5/6）。
- 10,000 个随机合法半回合无非法状态、无崩溃。
- 20 局 AI 自对弈 + 20 局人机（含执黑悔棋）无死锁。
- 三次重复/五次重复/50/75 回合/死局用例全部通过。
- AI 每步在设定时间预算 ±20% 内返回合法着法；搜索期间 UI 可响应。

### 工程

- fresh clone 在无 Blender 的环境下可 `--import`、跑测试、导出。
- CI 在 PR 上稳定通过；构建产物可下载运行。
- `main.gd` 拆分后单文件 < 600 行；规则引擎测试覆盖率（按关键分支）≥ 85%。
- 无 `D:\` 绝对路径、无空文件/空场景、LICENSE/CHANGELOG 齐全。

### 产品

- 1280×720、1280×800、1920×1080 下棋盘完整、UI 不重叠。
- 1080p 目标 60 FPS；加载 < 3 s；安装包 < 80 MB。
- 一局完整流程：开局 → 棋钟 → 走子 → 升变 → 认输/将死 → 结算 → 复盘 PGN → 再战。
- 中英文均可读；无系统字体环境显示正常。

---

## 7. 风险与应对

| 风险 | 影响 | 应对 |
| --- | --- | --- |
| 范围失控（先做联网/美术） | 核心稳定性长期不达标 | Phase 0/1 设硬门槛；联网放到 Phase 3 |
| Godot/Blender 版本漂移 | 导入/导出结果不一致 | 锁定 4.7.2；提交 glb；CI 固定版本 |
| AI 加强后变慢 | 体验下降/主线程卡顿 | 时间预算 + 线程 + 分级；以“每步时限”而非固定深度验收 |
| 跨平台渲染差异 | Web/Linux 画面异常 | CI 构建 + 多平台截图冒烟；质量档位 |
| 素材/字体授权 | 无法合法分发 | 补 LICENSE；字体用 OFL；音频/模型来源记录 |
| 重构破坏现有玩法 | 回归 bug | 先补测试再拆 UI；按 Board/HUD 小块迁移 |

---

## 8. 本周执行清单（建议）

- [ ] 建 `docs/chessset-next-plan` 分支与本文档（已完成）。
- [ ] 加 `tests/run_tests.gd`，把现有 perft 固化为第一批用例。
- [ ] 修 `_undo()` 人机执黑死锁，补回归测试。
- [ ] 补 `is_dead_position()` 与三次重复/75 回合，补 FEN 用例。
- [ ] 导出 `ChessSet.glb`，删除 `export_presets.cfg` 的绝对路径，验证 fresh clone headless 导入。
- [ ] 加 GitHub Actions（导入 + 测试 + Windows artifact）。
- [ ] 修 1280×800 相机取景，重拍 `demo/selected/black_view` 截图。
- [ ] 补 LICENSE、CHANGELOG、根 README，清理空文件/空场景。
- [ ] 开 PR，按 P0 清单逐项 review。

---

## 9. 附录

### 9.1 关键文件

```text
ChessSet/
  project.godot
  scenes/main.tscn
  scripts/main.gd          # 待拆分：场景/UI/流程
  scripts/chess_game.gd    # 规则引擎（优先补测试）
  scripts/chess_ai.gd      # AI（后续异步/加强）
  assets/shaders/          # 木纹/桌面
  assets/audio/            # 程序化音效
  ChessSet.blend           # 模型源文件
  ChessSet.blend.import    # Godot 导入配置
  export_presets.cfg       # 含本机绝对路径，待清理
  screenshots/             # 现有截图（部分存在裁切）
```

### 9.2 建议保留的测试局面

```text
起始：rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
Kiwipete：r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
升变：4k3/P7/8/8/8/8/8/4K3 w - - 0 1
将死：rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3
同色象死局：8/8/8/8/8/8/8/KBkb4 w - - 0 1   # b1/d1 均为浅色格
```

### 9.3 常用命令

```bash
# 导入（无编辑器）
godot --headless --path ChessSet --import

# 规则/引擎测试（测试入口落地后）
godot --headless --path ChessSet --script tests/run_tests.gd

# 导出 Windows
godot --headless --path ChessSet --export-release "Windows Desktop" build/ChessSet.exe

# 导出 Web（新增预设后）
godot --headless --path ChessSet --export-release "Web" build/web/index.html
```

---

**一句话总结**：ChessSet 的“玩法骨架”已经成立，下一步最值得投入的不是更多功能，而是**把规则边界、测试、构建链和取景这些“看起来不炫但决定成败”的基础打牢**；在此基础上再补棋钟/PGN/设置，最后才考虑联网与内容扩展。
