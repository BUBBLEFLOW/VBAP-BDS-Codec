#include "cxw_spatial_demo_model.h"
#include "cxw_bds_export_bridge.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path outputDir = argc > 1 ? argv[1] : "cxw_demo_output";

    try {
        std::filesystem::create_directories(outputDir);

        cxw::EdgeSpatialDemoModel model;
        const cxw::DemoScene scene = model.generateDefaultScene();
        const std::string sceneJson = model.serializeBdsSceneJson(scene, true);

        const bool ok =
            writeTextFile(outputDir / "cxw_demo_scene.bds.json", sceneJson) &&
            writeTextFile(outputDir / "cxw_demo_authoring.json", model.serializeAuthoringJson(scene, true)) &&
            writeTextFile(outputDir / "speaker_layout_4_0_bds.json", model.serializeSpeakerLayout4_0(true)) &&
            writeTextFile(outputDir / "validation_summary.json", model.serializeValidationSummary(scene, true));

        if (!ok) {
            std::cerr << "failed to write one or more output files\n";
            return 1;
        }

        constexpr std::uint32_t kFrameCount = 96000;
        std::vector<float> silentStereo(kFrameCount * 2U, 0.0f);
        std::vector<cxw::TrackPcmView> tracks = {
            {1, silentStereo.data(), kFrameCount, 48000, 2, "bass"},
            {2, silentStereo.data(), kFrameCount, 48000, 2, "drums"},
            {3, silentStereo.data(), kFrameCount, 48000, 2, "music"},
            {4, silentStereo.data(), kFrameCount, 48000, 2, "vocal"},
        };

        cxw::BdsExportBridge bridge;
        cxw::BdsExportOptions exportOptions;
        exportOptions.outputBdsPath = (outputDir / "cxw_demo.bds").string();
        exportOptions.dryRunDirectory = (outputDir / "bds_export_dryrun").string();
        exportOptions.requireRealBdsFile = false;
        const auto exportResult = bridge.exportScene(sceneJson, tracks, exportOptions);
        if (!exportResult.ok) {
            std::cerr << "BDS export bridge failed: " << exportResult.error << "\n";
            return 1;
        }

        std::cout << "CXW edge spatial demo generated in: " << outputDir.string() << "\n";
        std::cout << "BDS scene: " << (outputDir / "cxw_demo_scene.bds.json").string() << "\n";
        std::cout << "BDS export manifest: " << exportResult.manifestPath << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "cxw_spatial_demo failed: " << ex.what() << "\n";
        return 1;
    }
}
