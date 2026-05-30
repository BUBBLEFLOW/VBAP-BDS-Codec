# BDS Export Bridge

这部分补上的是“JUCE App 内调用 bds-codec 写出真实 `.bds` 文件”的接口层。

## 新增文件

```text
cxw/src/cxw_bds_export_bridge.h
cxw/src/cxw_bds_export_bridge.cpp
cxw/web/bds_export_visualizer.html
cxw/data/bds_export_plan_demo.json
```

## 输入

`BdsExportBridge` 接收两类输入：

```text
1. sceneJson
   由 EdgeSpatialDemoModel 生成，结构参考 bds-codec scene JSON。

2. tracks
   JUCE 导出的每条 stem PCM。
   当前 demo 约束为 interleaved float32 stereo / 48000 Hz。
```

对应 C++ 数据结构：

```cpp
cxw::TrackPcmView {
    trackId;
    interleavedPcm;
    frameCount;
    sampleRate;
    channelCount;
    name;
}
```

## 输出

真实 bds-codec 模式输出：

```text
cxw_demo.bds
```

要求：

```powershell
cmake -S .\cxw\src -B .\cxw\build `
  -DCXW_HAS_BDS_CODEC=ON `
  -DCMAKE_PREFIX_PATH="<your rebuilt bds-codec install prefix>"
```

dry-run 模式输出：

```text
bds_export_dryrun/
├── scene_json_for_bds_codec.json
└── bds_export_manifest.json
```

dry-run 不写真实 `.bds`，但会验证 JUCE 侧是否已经准备好了 scene JSON 和 PCM track 映射。

## 调用顺序

真实编码时桥接层按这个顺序调用 bds-codec：

```cpp
bds::scene_json::parse(sceneJson, &scene, &error);
bds::validateScene(scene, &error);

bds::EncodeSession encoder;
encoder.setScene(scene);

for each track:
    encoder.setTrackPcm(trackId, pcm, frameCount, 48000, 2);

encoder.finish(outputPath);
```

## JUCE 接入点

推荐把导出放在非实时线程：

```cpp
cxw::EdgeSpatialDemoModel model;
const auto scene = model.generateDefaultScene();
const auto sceneJson = model.serializeBdsSceneJson(scene, true);

std::vector<cxw::TrackPcmView> tracks = {
    {1, bassPcm.data(), frameCount, 48000, 2, "bass"},
    {2, drumsPcm.data(), frameCount, 48000, 2, "drums"},
    {3, musicPcm.data(), frameCount, 48000, 2, "music"},
    {4, vocalPcm.data(), frameCount, 48000, 2, "vocal"},
};

cxw::BdsExportOptions options;
options.outputBdsPath = outputPath;
options.requireRealBdsFile = true;

cxw::BdsExportBridge bridge;
const auto result = bridge.exportScene(sceneJson, tracks, options);
```

不要在 `processBlock()` 里做 JSON、文件 IO 或 bds-codec 编码。

## 可视化

打开：

```text
cxw/web/bds_export_visualizer.html
```

它展示了输入、bds-codec 调用顺序、BDS 容器输出和 dry-run fallback。
