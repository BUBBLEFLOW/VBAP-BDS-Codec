# CXW Edge Spatial Demo

这个文件夹是一个两天内可落地的端侧最小验证包，用来把“生成式空间音频结果”收敛成可部署到 JUCE C++ 音频 App 的 demo。

MVP 范围刻意压小：模型不在端上跑大型神经网络，而是把已调研的生成式空间结果固化为一个轻量 C++ 规则模型，生成对象位置、增益、轨迹和 BDS scene JSON。音频轨道的 Opus 编码和最终 `.bds` 容器写入继续交给 `bds-codec`。

## 目录

```text
cxw/
├── src/                         # 纯 C++17 demo 模型和 CLI
├── data/                        # 已生成的 BDS scene JSON、authoring JSON、speaker layout
├── docs/                        # 设计说明、使用说明、JUCE 集成说明
├── scripts/                     # JSON 结构校验脚本
└── web/index.html               # 本地可直接打开的空间场景可视化
```

## 快速查看

1. 打开可视化：

```text
cxw/web/index.html
```

查看 JUCE 到 bds-codec 的导出流程：

```text
cxw/web/bds_export_visualizer.html
```

查看 `Bubbleflow Dynamic Space.app` 的 bundle 结构：

```text
cxw/web/bubbleflow_app_visualizer.html
```

查看本轮对话归档：

```text
cxw/web/conversation_archive.html
```

2. 校验示例 BDS scene JSON：

```powershell
powershell -ExecutionPolicy Bypass -File .\cxw\scripts\validate_scene.ps1 `
  -Scene .\cxw\data\cxw_demo_scene.bds.json `
  -Report .\cxw\data\validation_report.json
```

3. 有 C++17 编译器时生成同款输出：

```powershell
cmake -S .\cxw\src -B .\cxw\build
cmake --build .\cxw\build --config Release
.\cxw\build\cxw_spatial_demo_cli.exe .\cxw\generated
```

当前机器没有可用 C++ 编译器，所以上面构建命令作为工程用法保留；示例 JSON 已经放在 `data/`。

真实 `.bds` 导出需要平台匹配的 bds-codec 包：

```powershell
cmake -S .\cxw\src -B .\cxw\build `
  -DCXW_HAS_BDS_CODEC=ON `
  -DCMAKE_PREFIX_PATH="<bds-codec-install>"
cmake --build .\cxw\build --config Release
```

## 关键输出

- `data/cxw_demo_scene.bds.json`：参考 `bds-codec` v0 scene 要求的编码输入 JSON。
- `data/cxw_demo_authoring.json`：贴近 AO/JUCE authoring metadata 的对象层描述。
- `data/speaker_layout_4_0_bds.json`：用于 `SimpleRenderer` 或预览的 4 扬声器布局。
- `data/bubbleflow_app_manifest.json`：`Bubbleflow Dynamic Space.app` 的静态解析清单。
- `src/cxw_spatial_demo_model.*`：端侧可嵌入的最小 C++ 模型。
- `src/cxw_bds_export_bridge.*`：JUCE PCM + scene JSON 到 bds-codec 的导出桥接层。

## HTML 分工

`web/index.html` 是空间场景查看器。

输入：

```text
BDS scene JSON
```

输出：

```text
四扬声器布局
四个声音对象的位置
对象运动轨迹
当前时间点 azimuth / elevation / distance / gain
BDS 编码参数检查
```

它不播放声音、不生成 `.bds`，只做空间元数据可视化。

`web/bds_export_visualizer.html` 是导出流程查看器。

输入：

```text
bds_export_plan_demo.json
```

输出：

```text
JUCE 输入、BdsExportBridge、bds-codec 调用顺序、真实 .bds / dry-run 输出关系图
```

## JUCE 到 BDS 的输入输出

JUCE App 侧需要准备两类输入：

```text
1. scene JSON
   由 EdgeSpatialDemoModel 生成，描述 track、object、位置、轨迹、mix。

2. PCM tracks
   4 路 stereo interleaved float32 PCM。
```

当前 demo 的 track 约定：

```text
track 1: bass
track 2: drums
track 3: music
track 4: vocal
```

`BdsExportBridge` 的真实编码调用顺序：

```text
bds::scene_json::parse
bds::validateScene
bds::EncodeSession::setScene
bds::EncodeSession::setTrackPcm
bds::EncodeSession::finish
```

如果 `CXW_HAS_BDS_CODEC=ON` 且 bds-codec 可链接，输出：

```text
cxw_demo.bds
```

如果没有 bds-codec，输出 dry-run bundle：

```text
bds_export_dryrun/
├── scene_json_for_bds_codec.json
└── bds_export_manifest.json
```

dry-run 用来验证 scene JSON、PCM track 映射和导出链路，不是最终音频容器。

## 约束

- `sample_rate = 48000`
- `timebase = 48000`
- `frame_duration_ticks = 960`，即 20 ms
- `codec_id = opus`
- 4 个对象轨：bass、drums、music、vocal
- 每轨 stereo/interleaved PCM 交给 `bds-codec::EncodeSession`
