#include "cxw_spatial_demo_model.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cxw {
namespace {

constexpr double kPi = 3.14159265358979323846;

float clampFloat(float value, float low, float high) {
    return std::max(low, std::min(value, high));
}

double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

std::string escapeJson(const std::string& input) {
    std::ostringstream out;
    for (char c : input) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

std::string indent(int depth, bool pretty) {
    return pretty ? std::string(static_cast<std::size_t>(depth) * 2U, ' ') : std::string{};
}

std::string newline(bool pretty) {
    return pretty ? "\n" : "";
}

float linearToDb(float value) {
    const float safeValue = std::max(value, 0.000001f);
    return 20.0f * std::log10(safeValue);
}

Position offsetPosition(Position base, float azimuthOffset, float elevationOffset, float distanceOffset) {
    base.azimuthDeg = clampFloat(base.azimuthDeg + azimuthOffset, -180.0f, 179.9f);
    base.elevationDeg = clampFloat(base.elevationDeg + elevationOffset, -90.0f, 90.0f);
    base.distanceM = std::max(0.05f, base.distanceM + distanceOffset);
    return base;
}

void appendPositionObject(std::ostringstream& out, const Position& position, bool pretty, int depth) {
    const CartesianPosition cart = EdgeSpatialDemoModel::toBdsCartesian(position);
    out << "{" << newline(pretty);
    out << indent(depth + 1, pretty) << "\"x\": " << cart.x << "," << newline(pretty);
    out << indent(depth + 1, pretty) << "\"y\": " << cart.y << "," << newline(pretty);
    out << indent(depth + 1, pretty) << "\"z\": " << cart.z << "," << newline(pretty);
    out << indent(depth + 1, pretty) << "\"azimuth_deg\": " << position.azimuthDeg << "," << newline(pretty);
    out << indent(depth + 1, pretty) << "\"elevation_deg\": " << position.elevationDeg << "," << newline(pretty);
    out << indent(depth + 1, pretty) << "\"distance\": " << position.distanceM << newline(pretty);
    out << indent(depth, pretty) << "}";
}

}  // namespace

EdgeSpatialDemoModel::EdgeSpatialDemoModel(ModelOptions options)
    : options_(options) {
    std::string error;
    if (!validateOptions(options_, &error)) {
        throw std::invalid_argument(error);
    }
}

const ModelOptions& EdgeSpatialDemoModel::options() const noexcept {
    return options_;
}

bool EdgeSpatialDemoModel::validateOptions(const ModelOptions& options, std::string* error) {
    auto fail = [error](const std::string& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (options.sampleRate != 48000) {
        return fail("BDS v0 demo requires sampleRate == 48000");
    }
    if (options.timebase != 48000) {
        return fail("BDS v0 demo requires timebase == 48000");
    }
    if (options.frameDurationTicks != 480 && options.frameDurationTicks != 960) {
        return fail("frameDurationTicks must be 480 or 960");
    }
    if (options.durationTicks == 0) {
        return fail("durationTicks must be greater than zero");
    }
    if (options.nominalBitrate == 0) {
        return fail("nominalBitrate must be greater than zero");
    }
    return true;
}

CartesianPosition EdgeSpatialDemoModel::toBdsCartesian(const Position& position) {
    const double azimuthRad = degToRad(position.azimuthDeg);
    const double elevationRad = degToRad(position.elevationDeg);
    const double horizontal = static_cast<double>(position.distanceM) * std::cos(elevationRad);

    CartesianPosition result;
    result.x = static_cast<float>(horizontal * std::sin(azimuthRad));
    result.y = static_cast<float>(static_cast<double>(position.distanceM) * std::sin(elevationRad));
    result.z = static_cast<float>(-horizontal * std::cos(azimuthRad));
    return result;
}

std::vector<ObjectPreset> EdgeSpatialDemoModel::defaultPresets() const {
    return {
        {1, 1, "bass", "bass", "medium", {-12.0f, -2.0f, 0.78f}, -3.0f, 70.0f},
        {2, 2, "drums", "drums", "medium", {-34.0f, 0.0f, 0.74f}, -1.2f, 95.0f},
        {3, 3, "music", "music", "medium", {42.0f, 8.0f, 0.88f}, -3.0f, 110.0f},
        {4, 4, "vocal", "vocal", "high", {0.0f, 4.0f, 0.62f}, 0.0f, 42.0f},
    };
}

DemoScene EdgeSpatialDemoModel::generateDefaultScene() const {
    DemoScene scene;
    scene.options = options_;

    const auto presets = defaultPresets();
    scene.objects.reserve(presets.size());

    for (const auto& preset : presets) {
        GeneratedObject object;
        object.preset = preset;

        const std::uint64_t middleTicks = options_.durationTicks / 2U;
        const float azimuthSwing = preset.semanticTag == "vocal" ? 3.0f :
                                   preset.semanticTag == "bass" ? 12.0f :
                                   preset.semanticTag == "drums" ? 7.0f : 28.0f;
        const float elevationSwing = preset.semanticTag == "music" ? 4.0f :
                                     preset.semanticTag == "vocal" ? 1.0f : 0.0f;

        object.automation.push_back({0U, preset.anchor, preset.gainDb});
        object.automation.push_back({
            middleTicks,
            offsetPosition(preset.anchor, azimuthSwing, elevationSwing, 0.0f),
            preset.gainDb
        });
        object.automation.push_back({
            options_.durationTicks,
            offsetPosition(preset.anchor, -azimuthSwing * 0.35f, 0.0f, 0.0f),
            preset.gainDb
        });

        scene.objects.push_back(object);
    }

    return scene;
}

DemoScene EdgeSpatialDemoModel::generateFromStemRms(const std::array<float, 4>& stemRms) const {
    DemoScene scene = generateDefaultScene();

    for (std::size_t i = 0; i < scene.objects.size() && i < stemRms.size(); ++i) {
        const float loudnessDb = clampFloat(linearToDb(stemRms[i] / 0.18f), -9.0f, 3.0f);
        const float distanceNudge = clampFloat(-loudnessDb * 0.0125f, -0.04f, 0.08f);

        for (auto& keyframe : scene.objects[i].automation) {
            keyframe.gainDb = clampFloat(scene.objects[i].preset.gainDb + loudnessDb * 0.35f, -12.0f, 4.0f);
            keyframe.position.distanceM = std::max(0.05f, keyframe.position.distanceM + distanceNudge);
        }
    }

    return scene;
}

std::string EdgeSpatialDemoModel::serializeBdsSceneJson(const DemoScene& scene, bool pretty) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);

    out << "{" << newline(pretty);
    out << indent(1, pretty) << "\"version\": 1," << newline(pretty);
    out << indent(1, pretty) << "\"sample_rate\": " << scene.options.sampleRate << "," << newline(pretty);
    out << indent(1, pretty) << "\"timebase\": " << scene.options.timebase << "," << newline(pretty);

    out << indent(1, pretty) << "\"authoring_metadata\": ";
    out << serializeAuthoringJson(scene, pretty);
    out << "," << newline(pretty);

    out << indent(1, pretty) << "\"codec_configs\": [" << newline(pretty);
    out << indent(2, pretty) << "{" << newline(pretty);
    out << indent(3, pretty) << "\"codec_config_id\": " << scene.options.codecConfigId << "," << newline(pretty);
    out << indent(3, pretty) << "\"codec_id\": \"opus\"," << newline(pretty);
    out << indent(3, pretty) << "\"sample_rate\": " << scene.options.sampleRate << "," << newline(pretty);
    out << indent(3, pretty) << "\"channel_count\": 2," << newline(pretty);
    out << indent(3, pretty) << "\"frame_duration_ticks\": " << scene.options.frameDurationTicks << "," << newline(pretty);
    out << indent(3, pretty) << "\"nominal_bitrate\": " << scene.options.nominalBitrate << newline(pretty);
    out << indent(2, pretty) << "}" << newline(pretty);
    out << indent(1, pretty) << "]," << newline(pretty);

    out << indent(1, pretty) << "\"tracks\": [" << newline(pretty);
    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
        const auto& object = scene.objects[i];
        out << indent(2, pretty) << "{" << newline(pretty);
        out << indent(3, pretty) << "\"track_id\": " << object.preset.trackId << "," << newline(pretty);
        out << indent(3, pretty) << "\"codec_config_id\": " << scene.options.codecConfigId << "," << newline(pretty);
        out << indent(3, pretty) << "\"element_id\": " << object.preset.elementId << "," << newline(pretty);
        out << indent(3, pretty) << "\"channel_count\": 2," << newline(pretty);
        out << indent(3, pretty) << "\"flags\": 0," << newline(pretty);
        out << indent(3, pretty) << "\"name\": \"" << escapeJson(object.preset.name) << "\"" << newline(pretty);
        out << indent(2, pretty) << "}" << (i + 1U < scene.objects.size() ? "," : "") << newline(pretty);
    }
    out << indent(1, pretty) << "]," << newline(pretty);

    out << indent(1, pretty) << "\"elements\": [" << newline(pretty);
    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
        const auto& object = scene.objects[i];
        out << indent(2, pretty) << "{" << newline(pretty);
        out << indent(3, pretty) << "\"element_id\": " << object.preset.elementId << "," << newline(pretty);
        out << indent(3, pretty) << "\"name\": \"" << escapeJson(object.preset.name) << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"type\": \"object\"," << newline(pretty);
        out << indent(3, pretty) << "\"semantic_tag\": \"" << escapeJson(object.preset.semanticTag) << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"importance\": \"" << escapeJson(object.preset.importance) << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"is_bed\": false," << newline(pretty);
        out << indent(3, pretty) << "\"track_ids\": [" << object.preset.trackId << "]," << newline(pretty);
        out << indent(3, pretty) << "\"default_position\": ";
        appendPositionObject(out, object.automation.front().position, pretty, 3);
        out << "," << newline(pretty);
        out << indent(3, pretty) << "\"automation\": [" << newline(pretty);
        for (std::size_t k = 0; k < object.automation.size(); ++k) {
            const auto& keyframe = object.automation[k];
            out << indent(4, pretty) << "{" << newline(pretty);
            out << indent(5, pretty) << "\"time_ticks\": " << keyframe.timeTicks << "," << newline(pretty);
            out << indent(5, pretty) << "\"position\": ";
            appendPositionObject(out, keyframe.position, pretty, 5);
            out << "," << newline(pretty);
            out << indent(5, pretty) << "\"gain_db\": " << keyframe.gainDb << newline(pretty);
            out << indent(4, pretty) << "}" << (k + 1U < object.automation.size() ? "," : "") << newline(pretty);
        }
        out << indent(3, pretty) << "]" << newline(pretty);
        out << indent(2, pretty) << "}" << (i + 1U < scene.objects.size() ? "," : "") << newline(pretty);
    }
    out << indent(1, pretty) << "]," << newline(pretty);

    out << indent(1, pretty) << "\"mix_presentations\": [" << newline(pretty);
    out << indent(2, pretty) << "{" << newline(pretty);
    out << indent(3, pretty) << "\"mix_id\": 1," << newline(pretty);
    out << indent(3, pretty) << "\"name\": \"cxw_edge_demo\"," << newline(pretty);
    out << indent(3, pretty) << "\"headroom_db\": " << scene.options.headroomDb << "," << newline(pretty);
    out << indent(3, pretty) << "\"element_ids\": [";
    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
        out << scene.objects[i].preset.elementId << (i + 1U < scene.objects.size() ? ", " : "");
    }
    out << "]" << newline(pretty);
    out << indent(2, pretty) << "}" << newline(pretty);
    out << indent(1, pretty) << "]" << newline(pretty);
    out << "}" << newline(pretty);

    return out.str();
}

std::string EdgeSpatialDemoModel::serializeAuthoringJson(const DemoScene& scene, bool pretty) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);

    out << "{" << newline(pretty);
    out << indent(1, pretty) << "\"scene_id\": \"cxw_edge_spatial_demo_001\"," << newline(pretty);
    out << indent(1, pretty) << "\"sample_rate\": " << scene.options.sampleRate << "," << newline(pretty);
    out << indent(1, pretty) << "\"duration_sec\": " << static_cast<double>(scene.options.durationTicks) / scene.options.timebase << "," << newline(pretty);
    out << indent(1, pretty) << "\"notes\": \"CXW edge spatial demo generated for JUCE/BDS integration\"," << newline(pretty);
    out << indent(1, pretty) << "\"objects\": [" << newline(pretty);

    for (std::size_t i = 0; i < scene.objects.size(); ++i) {
        const auto& object = scene.objects[i];
        const auto& anchor = object.automation.front().position;
        const double azimuthRad = degToRad(anchor.azimuthDeg);
        const double stageX = static_cast<double>(anchor.distanceM) * std::sin(azimuthRad);
        const double stageY = static_cast<double>(anchor.distanceM) * std::cos(azimuthRad);

        out << indent(2, pretty) << "{" << newline(pretty);
        out << indent(3, pretty) << "\"object_id\": \"obj_" << std::setw(3) << std::setfill('0') << object.preset.elementId << std::setfill(' ') << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"label\": \"" << escapeJson(object.preset.name) << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"audio_path\": \"juce_track_" << object.preset.trackId << "\"," << newline(pretty);
        out << indent(3, pretty) << "\"gain\": " << std::pow(10.0, object.preset.gainDb / 20.0) << "," << newline(pretty);
        out << indent(3, pretty) << "\"spread\": " << object.preset.spreadDeg / 360.0f << "," << newline(pretty);
        out << indent(3, pretty) << "\"depth\": " << anchor.distanceM << "," << newline(pretty);
        out << indent(3, pretty) << "\"distance\": " << anchor.distanceM << "," << newline(pretty);
        out << indent(3, pretty) << "\"azimuth_deg\": " << anchor.azimuthDeg << "," << newline(pretty);
        out << indent(3, pretty) << "\"elevation_deg\": " << anchor.elevationDeg << "," << newline(pretty);
        out << indent(3, pretty) << "\"spread_deg\": " << object.preset.spreadDeg << "," << newline(pretty);
        out << indent(3, pretty) << "\"position\": [" << stageX << ", " << stageY << "]," << newline(pretty);
        out << indent(3, pretty) << "\"trajectory\": [" << newline(pretty);
        for (std::size_t k = 0; k < object.automation.size(); ++k) {
            const auto& keyframe = object.automation[k];
            out << indent(4, pretty) << "{ \"time_ticks\": " << keyframe.timeTicks
                << ", \"azimuth_deg\": " << keyframe.position.azimuthDeg
                << ", \"elevation_deg\": " << keyframe.position.elevationDeg
                << ", \"distance\": " << keyframe.position.distanceM
                << ", \"gain_db\": " << keyframe.gainDb << " }"
                << (k + 1U < object.automation.size() ? "," : "") << newline(pretty);
        }
        out << indent(3, pretty) << "]," << newline(pretty);
        out << indent(3, pretty) << "\"tags\": [\"" << escapeJson(object.preset.semanticTag) << "\", \"generated\"]," << newline(pretty);
        out << indent(3, pretty) << "\"mute\": false," << newline(pretty);
        out << indent(3, pretty) << "\"solo\": false" << newline(pretty);
        out << indent(2, pretty) << "}" << (i + 1U < scene.objects.size() ? "," : "") << newline(pretty);
    }

    out << indent(1, pretty) << "]" << newline(pretty);
    out << "}";
    return out.str();
}

std::string EdgeSpatialDemoModel::serializeSpeakerLayout4_0(bool pretty) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << "{" << newline(pretty);
    out << indent(1, pretty) << "\"speakers\": [" << newline(pretty);

    const std::array<Position, 4> positions = {{
        {-45.0f, 0.0f, 1.0f},
        {45.0f, 0.0f, 1.0f},
        {-135.0f, 0.0f, 1.0f},
        {135.0f, 0.0f, 1.0f},
    }};

    for (std::size_t i = 0; i < positions.size(); ++i) {
        out << indent(2, pretty) << "{" << newline(pretty);
        out << indent(3, pretty) << "\"speaker_id\": " << (i + 1U) << "," << newline(pretty);
        out << indent(3, pretty) << "\"position\": ";
        appendPositionObject(out, positions[i], pretty, 3);
        out << newline(pretty) << indent(2, pretty) << "}" << (i + 1U < positions.size() ? "," : "") << newline(pretty);
    }

    out << indent(1, pretty) << "]" << newline(pretty);
    out << "}" << newline(pretty);
    return out.str();
}

std::string EdgeSpatialDemoModel::serializeValidationSummary(const DemoScene& scene, bool pretty) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);

    std::string error;
    const bool optionsValid = validateOptions(scene.options, &error);

    out << "{" << newline(pretty);
    out << indent(1, pretty) << "\"model_id\": \"cxw_edge_spatial_mvp_v0\"," << newline(pretty);
    out << indent(1, pretty) << "\"options_valid\": " << (optionsValid ? "true" : "false") << "," << newline(pretty);
    out << indent(1, pretty) << "\"error\": \"" << escapeJson(optionsValid ? "" : error) << "\"," << newline(pretty);
    out << indent(1, pretty) << "\"object_count\": " << scene.objects.size() << "," << newline(pretty);
    out << indent(1, pretty) << "\"frame_duration_ms\": " << (1000.0 * scene.options.frameDurationTicks / scene.options.timebase) << "," << newline(pretty);
    out << indent(1, pretty) << "\"duration_sec\": " << static_cast<double>(scene.options.durationTicks) / scene.options.timebase << "," << newline(pretty);
    out << indent(1, pretty) << "\"bds_constraints\": {" << newline(pretty);
    out << indent(2, pretty) << "\"sample_rate_48000\": " << (scene.options.sampleRate == 48000 ? "true" : "false") << "," << newline(pretty);
    out << indent(2, pretty) << "\"timebase_48000\": " << (scene.options.timebase == 48000 ? "true" : "false") << "," << newline(pretty);
    out << indent(2, pretty) << "\"opus_20ms_or_10ms\": " << ((scene.options.frameDurationTicks == 960 || scene.options.frameDurationTicks == 480) ? "true" : "false") << "," << newline(pretty);
    out << indent(2, pretty) << "\"max_four_objects\": " << (scene.objects.size() <= 4U ? "true" : "false") << newline(pretty);
    out << indent(1, pretty) << "}" << newline(pretty);
    out << "}" << newline(pretty);
    return out.str();
}

}  // namespace cxw
