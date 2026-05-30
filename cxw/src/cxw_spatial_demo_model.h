#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cxw {

struct Position {
    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    float distanceM = 1.0f;
};

struct CartesianPosition {
    float x = 0.0f;
    float y = 0.0f;
    float z = -1.0f;
};

struct AutomationKeyframe {
    std::uint64_t timeTicks = 0;
    Position position{};
    float gainDb = 0.0f;
};

struct ObjectPreset {
    std::uint32_t trackId = 0;
    std::uint32_t elementId = 0;
    std::string name;
    std::string semanticTag;
    std::string importance = "medium";
    Position anchor{};
    float gainDb = 0.0f;
    float spreadDeg = 60.0f;
};

struct GeneratedObject {
    ObjectPreset preset{};
    std::vector<AutomationKeyframe> automation;
};

struct ModelOptions {
    std::uint32_t sampleRate = 48000;
    std::uint32_t timebase = 48000;
    std::uint32_t frameDurationTicks = 960;
    std::uint32_t durationTicks = 96000;
    std::uint32_t codecConfigId = 1;
    std::uint32_t nominalBitrate = 128000;
    float headroomDb = 6.0f;
};

struct DemoScene {
    ModelOptions options{};
    std::vector<GeneratedObject> objects;
};

class EdgeSpatialDemoModel {
public:
    explicit EdgeSpatialDemoModel(ModelOptions options = {});

    [[nodiscard]] DemoScene generateDefaultScene() const;

    // RMS inputs are linear amplitudes for bass, drums, music, vocal.
    // The method keeps the same object layout and nudges gain/distances only.
    [[nodiscard]] DemoScene generateFromStemRms(const std::array<float, 4>& stemRms) const;

    [[nodiscard]] std::string serializeBdsSceneJson(const DemoScene& scene, bool pretty = true) const;
    [[nodiscard]] std::string serializeAuthoringJson(const DemoScene& scene, bool pretty = true) const;
    [[nodiscard]] std::string serializeSpeakerLayout4_0(bool pretty = true) const;
    [[nodiscard]] std::string serializeValidationSummary(const DemoScene& scene, bool pretty = true) const;

    [[nodiscard]] const ModelOptions& options() const noexcept;

    static bool validateOptions(const ModelOptions& options, std::string* error);
    static CartesianPosition toBdsCartesian(const Position& position);

private:
    ModelOptions options_{};

    [[nodiscard]] std::vector<ObjectPreset> defaultPresets() const;
};

}  // namespace cxw
