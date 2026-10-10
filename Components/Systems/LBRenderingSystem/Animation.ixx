// Animation.ixx - Skeletal animation sampling and skinning matrices (pure math, no graphics API)
export module LB.RenderingSystem:Animation;

import std;
import :Camera;
import :Model;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// Local transform of one joint (relative to its parent)
    /// </summary>
    struct JointPose {
        Vector3 translation{};
        Quaternion rotation{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    /// <summary>
    /// The skeleton's rest pose (the joints' node transforms)
    /// </summary>
    [[nodiscard]] inline std::vector<JointPose> RestPose(const Skeleton& skeleton) {
        std::vector<JointPose> pose;
        pose.reserve(skeleton.joints.size());
        for (const auto& joint : skeleton.joints) {
            pose.push_back({ joint.translation, joint.rotation, joint.scale });
        }
        return pose;
    }

    /// <summary>
    /// Pose a skeleton with a clip at a time: joints the clip does not drive keep their rest pose.
    /// Looping clips wrap the time; others clamp it to [0, duration].
    /// </summary>
    [[nodiscard]] inline std::vector<JointPose> SampleClip(const Skeleton& skeleton, const AnimationClip& clip, float timeSeconds, bool loop) {
        auto pose = RestPose(skeleton);

        float time = std::max(0.0f, timeSeconds);
        if (clip.durationSeconds > 0.0f) {
            time = loop ? std::fmod(time, clip.durationSeconds) : std::min(time, clip.durationSeconds);
        }

        for (const auto& channel : clip.channels) {
            const auto joint = skeleton.FindJoint(channel.joint);
            const std::size_t width = channel.path == AnimationPath::Rotation ? 4 : 3;
            if (!joint || channel.times.empty() || channel.values.size() < channel.times.size() * width) continue;

            // Keys around the time: the last key at or before it and the first one after it
            // (the first or last key alone before the first or after the last key)
            const auto next = static_cast<std::size_t>(std::ranges::upper_bound(channel.times, time) - channel.times.begin());
            const std::size_t after = std::min(next, channel.times.size() - 1);
            const std::size_t before = next == 0 ? 0 : std::min(next - 1, after);
            const float span = channel.times[after] - channel.times[before];
            const float t = span > 0.0f ? std::clamp((time - channel.times[before]) / span, 0.0f, 1.0f) : 0.0f;

            const float* a = &channel.values[before * width];
            const float* b = &channel.values[after * width];
            auto& target = pose[*joint];
            if (channel.path == AnimationPath::Rotation) {
                target.rotation = Quaternion::Slerp({ a[0], a[1], a[2], a[3] }, { b[0], b[1], b[2], b[3] }, t);
                continue;
            }

            const Vector3 value{ a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t };
            if (channel.path == AnimationPath::Translation) target.translation = value;
            else target.scale = value;
        }
        return pose;
    }

    /// <summary>
    /// Mix pose b into pose a: weight 0 keeps a, 1 gives b
    /// </summary>
    [[nodiscard]] inline std::vector<JointPose> BlendPoses(const std::vector<JointPose>& a, const std::vector<JointPose>& b, float weight) {
        const float w = std::clamp(weight, 0.0f, 1.0f);
        const auto lerp = [w](Vector3 from, Vector3 to) {
            return Vector3{ from.x + (to.x - from.x) * w, from.y + (to.y - from.y) * w, from.z + (to.z - from.z) * w };
        };

        auto result = a;
        for (std::size_t i = 0; i < result.size() && i < b.size(); ++i) {
            result[i].translation = lerp(a[i].translation, b[i].translation);
            result[i].rotation = Quaternion::Slerp(a[i].rotation, b[i].rotation, w);
            result[i].scale = lerp(a[i].scale, b[i].scale);
        }
        return result;
    }

    /// <summary>
    /// Joint transforms in model space (joint space -> model space) for a pose
    /// </summary>
    [[nodiscard]] inline std::vector<Matrix4> ComputeJointTransforms(const Skeleton& skeleton, const std::vector<JointPose>& pose) {
        std::vector<Matrix4> transforms(skeleton.joints.size());
        for (const auto index : skeleton.evaluationOrder) {
            const auto& joint = skeleton.joints[index];
            const auto& local = pose[index];
            const auto matrix = Matrix4::FromTranslationRotationScale(local.translation, local.rotation, local.scale);
            transforms[index] = joint.parent >= 0 ? matrix * transforms[static_cast<std::size_t>(joint.parent)] : matrix * joint.rootTransform;
        }
        return transforms;
    }

    /// <summary>
    /// Skinning matrices (bind-pose model space -> posed model space), one per joint
    /// </summary>
    [[nodiscard]] inline std::vector<Matrix4> ComputeSkinMatrices(const Skeleton& skeleton, const std::vector<Matrix4>& jointTransforms) {
        std::vector<Matrix4> skin(skeleton.joints.size());
        for (std::size_t i = 0; i < skin.size(); ++i) {
            skin[i] = skeleton.joints[i].inverseBind * jointTransforms[i];
        }
        return skin;
    }

    /// <summary>
    /// The pose an AnimationState describes, given a way to look clips up (null: rest pose)
    /// </summary>
    template<typename FindClip>
    [[nodiscard]] std::vector<JointPose> PoseFor(const Skeleton& skeleton, const AnimationState& state, const FindClip& findClip) {
        const auto sample = [&](const AnimationLayer& layer) {
            const AnimationClip* clip = findClip(layer.clip);
            return clip ? SampleClip(skeleton, *clip, layer.timeSeconds, layer.loop) : RestPose(skeleton);
        };

        auto current = sample(state.current);
        if (state.blend >= 1.0f || state.previous.clip == InvalidAnimationId) return current;
        return BlendPoses(sample(state.previous), current, state.blend);
    }

} // namespace LB::Systems::Rendering
