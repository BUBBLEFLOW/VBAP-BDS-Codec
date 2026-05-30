# JUCE / BDS 集成说明

## 推荐架构

```text
JUCE UI / command
    │
    ▼
CXW EdgeSpatialDemoModel
    │ scene JSON
    ▼
bds-codec scene_json::parse + validateScene
    │
    ├── track PCM from JUCE buffers
    ▼
bds::EncodeSession
    │
    ▼
out.bds
```

实时监听链路可以继续走现有 VBAP 渲染器：

```text
object position automation
    ▼
VBAP gain / smoothing
    ▼
speaker mix
    ▼
streaming limiter
    ▼
JUCE output
```

## 最小接入代码

```cpp
#include "cxw_spatial_demo_model.h"

class ExportController {
public:
    std::string makeSceneJson() {
        cxw::EdgeSpatialDemoModel model;
        const auto demoScene = model.generateDefaultScene();
        return model.serializeBdsSceneJson(demoScene, true);
    }
};
```

带 stem RMS 的版本：

```cpp
std::array<float, 4> rms = {
    bassRms,
    drumsRms,
    musicRms,
    vocalRms,
};

cxw::EdgeSpatialDemoModel model;
const auto scene = model.generateFromStemRms(rms);
const auto sceneJson = model.serializeBdsSceneJson(scene, true);
```

## bds-codec 编码位置

BDS 编码建议放在用户触发导出时：

```cpp
// Pseudocode; final headers/signatures follow the rebuilt bds-codec package.
bds::Scene bdsScene;
std::string error;

auto status = bds::scene_json::parse(sceneJson, &bdsScene, &error);
if (!bds::ok(status)) {
    // show error on UI thread
}

status = bds::validateScene(bdsScene, &error);
if (!bds::ok(status)) {
    // show error on UI thread
}

bds::EncodeSession encoder;
encoder.setScene(bdsScene);

for (const auto& stem : exportedStems) {
    encoder.setTrackPcm(
        stem.trackId,
        stem.interleavedPcm.data(),
        stem.frameCount,
        48000,
        2
    );
}

encoder.finish(outputPath);
```

## 采样率策略

当前 BDS v0 demo 固定 48 kHz。JUCE 里建议：

- `prepareToPlay()` 中检查宿主采样率。
- 如果设备不是 48 kHz，录制或导出前统一重采样到 48 kHz。
- scene 的 `timebase` 仍保持 48000。
- 自动化关键帧的 `time_ticks = seconds * 48000`。

## 与现有 VBAP / limiter 的关系

`EdgeSpatialDemoModel` 只生成对象元数据。它不替代：

- VBAP 增益计算
- 模式切换平滑
- 输出 limiter
- BDS 容器 CRC / index / Opus 编码

这些仍应复用 YJJ 交接中的 `VBAP_3D_CPP` 和 `BDS-Codec`。

