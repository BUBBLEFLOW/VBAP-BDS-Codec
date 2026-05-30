# CXW 端侧生成式空间音频 Demo 设计说明

## 目标

在周末两天内搭出一个最小可验证 demo：端上 C++ 生成空间对象元数据，输出符合 `bds-codec` 编码入口要求的 scene JSON，并能在 JUCE App 里接到录音、stem 或实时轨道后导出 `.bds`。

这个 demo 的重点不是云端生成音频，也不是训练新模型，而是把生成式空间音频调研结果压缩为一个端侧可运行的“空间编排模型”：

- 输入：4 条语义音轨或 stem，默认为 `bass/drums/music/vocal`。
- 可选输入：每条 stem 的 RMS，用于轻微调整增益和距离。
- 输出：BDS scene JSON、authoring metadata、自动化关键帧。
- 端侧条件：C++17、无第三方依赖、可放在 JUCE 工程，不要求 ML runtime。

## 已结合的本地资料

- `YJJ工作交接/交接文档.docx`：交接主线，确认 VBAP、BDS、App 编辑模式、ADM 母带格式四块工作。
- `YJJ工作交接/BDS 空间音频容器格式技术文档.docx`：确认 BDS 是 chunk-based 容器，包含 scene JSON、media segment、CRC 和索引。
- `BDS-Codec-main.zip/bds_codec_cpp_api.md`：确认 `sample_rate=48000`、`timebase>0`、`frame_duration_ticks=480/960`、Opus 轨道、`tracks/elements/mix_presentations` 结构。
- `VBAP_3D_CPP-main.zip/renderer_cpp_api.md`：确认端侧渲染链路中 VBAP、平滑和 limiter 的接口边界。
- `YJJ工作交接/中间格式参考（初版）.docx`：确认中间格式应表达 object、bed、空间元数据和后续分发格式的映射。
- `YJJ工作交接/ADM BWF音频封装技术方案.docx`：确认母带侧以后可以映射到 ADM/BW64，但当前 MVP 先走 BDS scene。

## 模型定义

`EdgeSpatialDemoModel` 是一个确定性 procedural model：

1. 为每个语义 stem 选择空间锚点。
2. 生成 0 s、1 s、2 s 三个自动化关键帧。
3. 将球坐标转换为 BDS scene 中使用的 `x/y/z + azimuth/elevation/distance`。
4. 输出 `codec_configs`、`tracks`、`elements`、`mix_presentations`。

默认空间策略：

| stem | 语义 | 默认位置 | 设计意图 |
|---|---|---|---|
| bass | 低频骨架 | 略偏右前、低仰角 | 稳定，不做大幅移动 |
| drums | 节奏 | 右前区域 | 有轻微横向摆动 |
| music | 和声/伴奏 | 左前到侧前 | 更宽、更有空间感 |
| vocal | 主唱 | 正前方略高 | 重点对象，保持清晰 |

注：本项目坐标遵循交接规范，公开接口使用球坐标，角度单位为度，距离单位为米。

## BDS 输出映射

`data/cxw_demo_scene.bds.json` 按 `bds-codec` 示例和 API 文档组织：

```text
root
├── version: 1
├── sample_rate: 48000
├── timebase: 48000
├── authoring_metadata
├── codec_configs: opus / stereo / 20 ms / 128 kbps
├── tracks: track_id -> codec_config_id -> element_id
├── elements: object + track_ids + default_position + automation
└── mix_presentations: default mix + 6 dB headroom
```

编码侧建议使用：

- `codec_id = "opus"`
- `channel_count = 2`
- `frame_duration_ticks = 960`
- `nominal_bitrate = 128000`
- 每个 media temporal unit 对齐 20 ms，即 960 samples at 48 kHz

## 端侧部署条件

- 模型本体无外部依赖，只用 C++ 标准库。
- 模型生成 scene JSON 可在非音频线程完成。
- JUCE audio callback 内只做音频采集、已有参数读取和平滑，不做文件 IO、JSON 序列化或 BDS 编码。
- `.bds` 导出发生在停止录音、Bounce、Share 或 Save 动作中。
- 若实时预览空间化，使用现有 VBAP/limiter 模块；scene 生成模型不替代渲染器。

## 两天实施计划

Day 1 上午：
梳理 BDS scene JSON 结构、bds-codec API、VBAP/limiter 边界，确定 MVP 只生成 metadata，不做神经音频合成。

Day 1 下午：
完成 `EdgeSpatialDemoModel` 数据结构、默认语义锚点、自动化轨迹和 JSON 输出。

Day 2 上午：
补充 BDS 示例数据、speaker layout、PowerShell 校验脚本和 JUCE 集成说明。

Day 2 下午：
完成 HTML 可视化，验证 JSON 结构，整理说明文稿和后续接 bds-codec 的步骤。

## 验收标准

- `cxw/data/cxw_demo_scene.bds.json` 可以通过本地结构校验。
- scene 满足 BDS v0 关键约束：48 kHz、20 ms 或 10 ms frame、track/element 引用闭合。
- C++ 模型不依赖 JUCE 或 bds-codec，可单独放入 JUCE 工程。
- HTML 能展示对象位置、轨迹、对象列表和 BDS 编码约束。

## 当前边界

- 当前工作区的 `libbdscodec.a` 是 macOS Mach-O 静态库，Windows 下不能直接链接；因此本包输出的是可喂给 bds-codec 的 JSON，而不是直接生成二进制 `.bds`。
- 这里的“模型”是端侧空间编排模型，不是完整的生成式音频大模型。
- VBAP 生产级渲染、limiter、C++ 全链路联调应接入现有 `VBAP_3D_CPP` 和 `BDS-Codec` 工程继续完成。

