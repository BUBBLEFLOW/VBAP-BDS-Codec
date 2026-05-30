# 使用说明文稿

## 1. Demo 做了什么

这个 demo 在端侧生成一个 4 对象空间音频场景：

- `bass`
- `drums`
- `music`
- `vocal`

它会给每个对象生成默认空间位置、2 秒自动化轨迹、增益、track 映射和 BDS 编码所需的 scene JSON。音频 PCM 或 Opus packet 不在这个模型内生成，仍由 JUCE App 和 `bds-codec` 的编码链路处理。

## 2. 直接使用现成输出

推荐先看这三个文件：

```text
cxw/data/cxw_demo_scene.bds.json
cxw/data/cxw_demo_authoring.json
cxw/data/speaker_layout_4_0_bds.json
```

`cxw_demo_scene.bds.json` 是后续交给 `bds-codec::EncodeSession` 的核心输入。

## 3. 校验 JSON

在项目根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\cxw\scripts\validate_scene.ps1 `
  -Scene .\cxw\data\cxw_demo_scene.bds.json `
  -Report .\cxw\data\validation_report.json
```

成功时会看到：

```text
validate_scene: PASS
```

## 4. 查看可视化

直接用浏览器打开：

```text
cxw/web/index.html
```

页面左侧是俯视空间场，右侧是对象表、BDS 编码约束和关键帧。拖动时间滑杆可以看到对象沿自动化轨迹移动。

也可以用右上角 `Load scene JSON` 加载更多 demo：

```text
cxw/data/demos/scene_static_square_4spk.bds.json
cxw/data/demos/scene_front_focus_4spk.bds.json
cxw/data/demos/scene_orbit_motion_4spk.bds.json
cxw/data/demos/scene_call_response_4spk.bds.json
```

## 5. 编译 C++ Demo

有 CMake 和 C++17 编译器时：

```powershell
cmake -S .\cxw\src -B .\cxw\build
cmake --build .\cxw\build --config Release
.\cxw\build\cxw_spatial_demo_cli.exe .\cxw\generated
```

CLI 会生成：

```text
cxw/generated/cxw_demo_scene.bds.json
cxw/generated/cxw_demo_authoring.json
cxw/generated/speaker_layout_4_0_bds.json
cxw/generated/validation_summary.json
cxw/generated/bds_export_dryrun/bds_export_manifest.json
```

### 5.1 查看 BDS 导出流程

打开：

```text
cxw/web/bds_export_visualizer.html
```

这个页面展示 JUCE App 里应该传给 bds-codec 的输入、调用顺序和输出。真实 `.bds` 需要启用 `CXW_HAS_BDS_CODEC` 并链接对应平台的 bds-codec。

## 6. 接入 JUCE App 的建议流程

1. 在 JUCE 工程中加入：

```text
cxw/src/cxw_spatial_demo_model.h
cxw/src/cxw_spatial_demo_model.cpp
```

2. 在非实时线程生成 scene：

```cpp
cxw::EdgeSpatialDemoModel model;
const auto scene = model.generateDefaultScene();
const auto json = model.serializeBdsSceneJson(scene, true);
```

3. 录制或导出时，把 JUCE 中每条 stem 的 interleaved float PCM 交给 bds-codec：

```cpp
bds::EncodeSession encoder;
encoder.setScene(sceneFromJson);
encoder.setTrackPcm(trackId, pcm.data(), frameCount, 48000, 2);
encoder.finish("out.bds");
```

实际函数签名以 `bds-codec` 安装头文件为准。本工作区目前只有 API 文档和 macOS 静态库，Windows 工程需要重新构建对应平台的 bds-codec。

## 7. 音频线程注意事项

- 不要在 `processBlock()` 中做 JSON 序列化。
- 不要在 `processBlock()` 中写 `.bds` 文件。
- 不要在 `processBlock()` 中动态分配大对象。
- 如果要实时预览位置变化，把关键帧提前转换成参数曲线，再在 audio callback 内只做插值和平滑。
- 输出前继续使用 VBAP 平滑和 limiter，避免切换或增益变化产生爆音。
