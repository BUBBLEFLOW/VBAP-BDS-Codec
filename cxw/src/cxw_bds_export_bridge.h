#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cxw {

struct TrackPcmView {
    std::uint32_t trackId = 0;
    const float* interleavedPcm = nullptr;
    std::uint32_t frameCount = 0;
    std::uint32_t sampleRate = 48000;
    std::uint16_t channelCount = 2;
    std::string name;
};

struct BdsExportOptions {
    std::string outputBdsPath = "cxw_demo.bds";
    std::string dryRunDirectory = "cxw_bds_export_dryrun";
    bool requireRealBdsFile = false;
    bool writeDryRunManifest = true;
};

struct BdsExportResult {
    bool ok = false;
    bool realBdsWritten = false;
    std::string outputBdsPath;
    std::string dryRunDirectory;
    std::string manifestPath;
    std::string error;
};

class BdsExportBridge {
public:
    [[nodiscard]] BdsExportResult exportScene(
        const std::string& sceneJson,
        const std::vector<TrackPcmView>& tracks,
        const BdsExportOptions& options
    ) const;

    [[nodiscard]] static bool validateTrackInputs(
        const std::vector<TrackPcmView>& tracks,
        std::string* error
    );

    [[nodiscard]] static std::string makeExportManifestJson(
        const std::string& sceneJson,
        const std::vector<TrackPcmView>& tracks,
        const BdsExportResult& result
    );
};

}  // namespace cxw
