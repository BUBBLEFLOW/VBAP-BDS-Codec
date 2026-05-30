#include "cxw_bds_export_bridge.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#if defined(CXW_HAS_BDS_CODEC) && CXW_HAS_BDS_CODEC
#include <bds/bds_encode_session.h>
#include <bds/scene_json.h>
#endif

namespace cxw {
namespace {

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

bool writeTextFile(const std::filesystem::path& path, const std::string& text, std::string* error) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error != nullptr) {
            *error = "failed to open file for writing: " + path.string();
        }
        return false;
    }
    out << text;
    return true;
}

std::uint64_t estimatePcmBytes(const std::vector<TrackPcmView>& tracks) {
    std::uint64_t total = 0;
    for (const auto& track : tracks) {
        total += static_cast<std::uint64_t>(track.frameCount) *
                 static_cast<std::uint64_t>(track.channelCount) *
                 static_cast<std::uint64_t>(sizeof(float));
    }
    return total;
}

}  // namespace

bool BdsExportBridge::validateTrackInputs(const std::vector<TrackPcmView>& tracks, std::string* error) {
    auto fail = [error](const std::string& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (tracks.empty()) {
        return fail("at least one PCM track is required");
    }

    for (const auto& track : tracks) {
        if (track.trackId == 0) {
            return fail("trackId must be non-zero");
        }
        if (track.interleavedPcm == nullptr) {
            return fail("track " + std::to_string(track.trackId) + " has null PCM pointer");
        }
        if (track.frameCount == 0) {
            return fail("track " + std::to_string(track.trackId) + " has zero frames");
        }
        if (track.sampleRate != 48000) {
            return fail("track " + std::to_string(track.trackId) + " must be 48000 Hz for BDS v0");
        }
        if (track.channelCount != 2) {
            return fail("track " + std::to_string(track.trackId) + " must be stereo for this demo");
        }
    }

    return true;
}

std::string BdsExportBridge::makeExportManifestJson(
    const std::string& sceneJson,
    const std::vector<TrackPcmView>& tracks,
    const BdsExportResult& result
) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"exporter\": \"cxw_bds_export_bridge\",\n";
    out << "  \"status\": \"" << (result.ok ? "ok" : "failed") << "\",\n";
    out << "  \"real_bds_written\": " << (result.realBdsWritten ? "true" : "false") << ",\n";
    out << "  \"output_bds_path\": \"" << escapeJson(result.outputBdsPath) << "\",\n";
    out << "  \"dry_run_directory\": \"" << escapeJson(result.dryRunDirectory) << "\",\n";
    out << "  \"error\": \"" << escapeJson(result.error) << "\",\n";
    out << "  \"scene_json_bytes\": " << sceneJson.size() << ",\n";
    out << "  \"estimated_pcm_bytes\": " << estimatePcmBytes(tracks) << ",\n";
    out << "  \"tracks\": [\n";
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        out << "    {\n";
        out << "      \"track_id\": " << track.trackId << ",\n";
        out << "      \"name\": \"" << escapeJson(track.name) << "\",\n";
        out << "      \"sample_rate\": " << track.sampleRate << ",\n";
        out << "      \"channel_count\": " << track.channelCount << ",\n";
        out << "      \"frame_count\": " << track.frameCount << ",\n";
        out << "      \"pcm_layout\": \"interleaved_f32\"\n";
        out << "    }" << (i + 1U < tracks.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"bds_codec_contract\": {\n";
    out << "    \"scene_parse\": \"bds::scene_json::parse(sceneJson, &scene, &error)\",\n";
    out << "    \"scene_validate\": \"bds::validateScene(scene, &error)\",\n";
    out << "    \"encode_order\": \"setScene -> setTrackPcm x N -> finish\",\n";
    out << "    \"sample_rate\": 48000,\n";
    out << "    \"frame_duration_ticks\": [480, 960]\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

BdsExportResult BdsExportBridge::exportScene(
    const std::string& sceneJson,
    const std::vector<TrackPcmView>& tracks,
    const BdsExportOptions& options
) const {
    BdsExportResult result;
    result.outputBdsPath = options.outputBdsPath;
    result.dryRunDirectory = options.dryRunDirectory;

    std::string error;
    if (sceneJson.empty()) {
        result.error = "scene JSON is empty";
        return result;
    }
    if (!validateTrackInputs(tracks, &error)) {
        result.error = error;
        return result;
    }

#if defined(CXW_HAS_BDS_CODEC) && CXW_HAS_BDS_CODEC
    {
        bds::Scene bdsScene;
        std::string parseError;
        auto status = bds::scene_json::parse(sceneJson, &bdsScene, &parseError);
        if (!bds::ok(status)) {
            result.error = "bds scene parse failed: " + parseError;
            return result;
        }

        parseError.clear();
        status = bds::validateScene(bdsScene, &parseError);
        if (!bds::ok(status)) {
            result.error = "bds scene validate failed: " + parseError;
            return result;
        }

        bds::EncodeSession encoder;
        status = encoder.setScene(bdsScene);
        if (!bds::ok(status)) {
            result.error = "bds encoder setScene failed";
            return result;
        }

        for (const auto& track : tracks) {
            status = encoder.setTrackPcm(
                track.trackId,
                track.interleavedPcm,
                track.frameCount,
                track.sampleRate,
                track.channelCount
            );
            if (!bds::ok(status)) {
                result.error = "bds encoder setTrackPcm failed for track " + std::to_string(track.trackId);
                return result;
            }
        }

        status = encoder.finish(options.outputBdsPath);
        if (!bds::ok(status)) {
            result.error = "bds encoder finish failed";
            return result;
        }

        result.ok = true;
        result.realBdsWritten = true;
    }
#else
    if (options.requireRealBdsFile) {
        result.error = "real .bds export requested, but CXW_HAS_BDS_CODEC is not enabled";
        return result;
    }

    result.ok = true;
    result.realBdsWritten = false;
#endif

    if (options.writeDryRunManifest || !result.realBdsWritten) {
        const std::filesystem::path dryRunDir(options.dryRunDirectory);
        const std::filesystem::path scenePath = dryRunDir / "scene_json_for_bds_codec.json";
        const std::filesystem::path manifestPath = dryRunDir / "bds_export_manifest.json";

        if (!writeTextFile(scenePath, sceneJson, &error)) {
            result.ok = false;
            result.error = error;
            return result;
        }
        if (!writeTextFile(manifestPath, makeExportManifestJson(sceneJson, tracks, result), &error)) {
            result.ok = false;
            result.error = error;
            return result;
        }
        result.manifestPath = manifestPath.string();
    }

    return result;
}

}  // namespace cxw
