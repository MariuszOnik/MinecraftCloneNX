#include "model/Animation.hpp"
#include "model/ModelMath.hpp"
#include "model/VoxelModel.hpp"
#include "model/VoxelModelLoader.hpp"
#include "model/VoxelModelMesher.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void Expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

bool Near(const float a, const float b) {
    return std::fabs(a - b) < 1.0e-4F;
}

constexpr std::string_view kChicken = R"({
  "name": "mini",
  "voxelSize": 0.0625,
  "palette": [
    { "name": "white", "color": [255, 255, 255] },
    { "name": "beak",  "color": [240, 176, 64, 255] }
  ],
  "parts": [
    { "name": "body", "parent": null, "position": [0, 4, 0], "pivot": [1, 0, 1],
      "size": [2, 2, 2], "fill": 1 },
    { "name": "head", "parent": "body", "position": [0, 2, 0], "pivot": [1, 0, 1],
      "size": [2, 2, 2], "voxels": [1, 0, 0, 1, 0, 1, 1, 0] }
  ]
})";

}  // namespace

int main() {
    using namespace voxelgame::vmodel;

    // Matrix: a child's world transform carries its parent's translation.
    {
        std::string error;
        const auto model = ParseVoxelModel(kChicken, error);
        Expect(model.has_value(), "valid model parses (" + error + ")");
        if (model) {
            Expect(model->parts.size() == 2, "two parts");
            Expect(model->parts[1].parent == 0, "head's parent resolves to body's index");
            Expect(model->Material(1) != nullptr && model->Material(1)->red == 255,
                   "palette index 1 is white");
            Expect(model->Material(3) == nullptr, "out-of-range palette index is null");

            const std::vector<Mat4> world = ResolvePartMatrices(*model, Mat4::Identity());
            // body at (0,4,0); head offset (0,2,0) -> head origin (0,6,0).
            const Vec3 headOrigin = world[1].TransformPoint({0.0F, 0.0F, 0.0F});
            Expect(Near(headOrigin.x, 0.0F) && Near(headOrigin.y, 6.0F) && Near(headOrigin.z, 0.0F),
                   "head world origin stacks on the body");

            // A 90-degree turn of the body about its pivot carries the head around.
            VoxelModel turned = *model;
            turned.parts[0].rotationDegrees = {0.0F, 90.0F, 0.0F};
            const std::vector<Mat4> spun = ResolvePartMatrices(turned, Mat4::Identity());
            const Vec3 h = spun[1].TransformPoint({0.0F, 0.0F, 0.0F});
            Expect(Near(h.y, 6.0F), "rotation about Y keeps the head's height");
        }
    }

    // Mesher: a solid 2x2x2 box is its 6 outer faces (4 quads each) and no
    // interior faces.
    {
        std::string error;
        const auto model = ParseVoxelModel(kChicken, error);
        Expect(model.has_value(), "model parses for mesher test");
        if (model) {
            const ModelMeshData body = BuildPartMesh(model->parts[0], *model);
            Expect(body.quadCount == 24, "solid 2^3 box emits 24 unit faces");
            Expect(body.VertexCount() == body.quadCount * 4, "four vertices per quad");
            Expect(body.TriangleCount() == body.quadCount * 2, "two triangles per quad");
            Expect(body.colors.size() == body.VertexCount() * 4, "rgba per vertex");

            const ModelMeshData head = BuildPartMesh(model->parts[1], *model);
            // 4 filled voxels, each fully exposed -> 24 faces.
            Expect(head.quadCount == 24, "four separated voxels expose 24 faces");
        }
    }

    // Loader rejects a malformed hierarchy and a bad palette index.
    {
        std::string error;
        Expect(!ParseVoxelModel(R"({"palette":[{"color":[1,2,3]}],
                "parts":[{"name":"a","parent":"missing","size":[1,1,1],"fill":1}]})",
                                error)
                    .has_value(),
               "unknown parent is rejected");
        Expect(!ParseVoxelModel(R"({"palette":[{"color":[1,2,3]}],
                "parts":[{"name":"a","size":[1,1,1],"fill":9}]})",
                                error)
                    .has_value(),
               "fill outside the palette is rejected");
        Expect(!ParseVoxelModel(R"({"palette":[{"color":[1,2,3]}],
                "parts":[{"name":"a","size":[2,1,1],"voxels":[1]}]})",
                                error)
                    .has_value(),
               "wrong voxel array length is rejected");
        Expect(!ParseVoxelModel(R"({"palette":[],"parts":[]})", error).has_value(),
               "empty palette is rejected");
        Expect(!ParseVoxelModel("not json", error).has_value(), "garbage is rejected");
    }

    // Animation: parse, sample, loop wrap, events, blend.
    {
        constexpr std::string_view kClip = R"({
          "name": "walk", "duration": 1.0, "loop": true,
          "tracks": [
            { "part": "body", "rotation": [
              { "t": 0.0, "value": [0, 0, 0] },
              { "t": 0.5, "value": [40, 0, 0] },
              { "t": 1.0, "value": [0, 0, 0] } ] },
            { "part": "ghost", "position": [ { "t": 0.0, "value": [9, 9, 9] } ] }
          ],
          "events": [ { "t": 0.25, "name": "footstep" }, { "t": 0.75, "name": "footstep" } ]
        })";

        std::string error;
        const auto clip = ParseAnimationClip(kClip, error);
        Expect(clip.has_value(), "valid clip parses (" + error + ")");

        constexpr std::string_view kModel = R"({
          "palette": [ { "color": [200, 200, 200] } ],
          "parts": [ { "name": "body", "parent": null, "size": [2, 2, 2], "fill": 1 } ]
        })";
        const auto model = ParseVoxelModel(kModel, error);
        Expect(model.has_value(), "clip test model parses");

        if (clip && model) {
            const std::vector<PartPose> atStart = SamplePose(*clip, *model, 0.0F);
            Expect(atStart.size() == 1, "pose has one entry per part");
            Expect(Near(atStart[0].rotationDegrees.x, 0.0F), "t=0 sits on the first key");

            const std::vector<PartPose> quarter = SamplePose(*clip, *model, 0.25F);
            Expect(Near(quarter[0].rotationDegrees.x, 20.0F),
                   "quarter way is halfway to the 0.5s key (40 deg)");

            // A track whose part is missing from the model is simply skipped.
            const std::vector<PartPose> past = SamplePose(*clip, *model, 5.0F);
            Expect(Near(past[0].rotationDegrees.x, 0.0F), "clamps past the last key");

            AnimationState state{&*clip, 0.9F, 1.0F};
            std::vector<std::string> fired;
            state.Advance(0.2F, &fired);  // 0.9 -> 1.1 -> wraps to 0.1
            Expect(Near(state.time, 0.1F), "a looping clip wraps its time");

            AnimationState ticking{&*clip, 0.2F, 1.0F};
            fired.clear();
            ticking.Advance(0.1F, &fired);  // crosses the 0.25s event
            Expect(fired.size() == 1 && fired[0] == "footstep",
                   "an event is reported once as the play head crosses it");

            AnimationState once{&*clip, 0.0F, 1.0F};
            AnimationClip oneShot = *clip;
            oneShot.loop = false;
            once.clip = &oneShot;
            once.Advance(2.0F);
            Expect(once.Finished(), "a non-looping clip finishes past its end");
        }

        Expect(!ParseAnimationClip(R"({"tracks":[]})", error).has_value(),
               "clip without duration is rejected");
        Expect(!ParseAnimationClip(R"({"duration":1.0,"tracks":[{"part":"x"}]})", error)
                    .has_value(),
               "track without keyframes is rejected");

        const std::vector<PartPose> a(2);
        std::vector<PartPose> b(2);
        b[0].rotationDegrees = {10.0F, 0.0F, 0.0F};
        const std::vector<PartPose> mid = BlendPoses(a, b, 0.5F);
        Expect(Near(mid[0].rotationDegrees.x, 5.0F), "BlendPoses is a linear midpoint");
    }

    if (failures == 0) {
        std::cout << "voxel model tests passed\n";
        return 0;
    }
    std::cerr << failures << " voxel model test(s) failed\n";
    return 1;
}
