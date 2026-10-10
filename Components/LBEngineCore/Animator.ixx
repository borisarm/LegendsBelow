// Animator.ixx - Clip playback with cross-fades for skinned characters
export module LB.Engine:Animator;

import std;
import LB.RenderingSystem;

export namespace LB::Engine {

    /// <summary>
    /// Plays animation clips on one skinned character: the current clip advances (looping or
    /// holding its last frame) at a playback speed, and switching clips cross-fades from the
    /// previous one. Works without loaded clips (InvalidAnimationId): the state then shows the
    /// rest pose, so gameplay never depends on animation data.
    /// </summary>
    class Animator {
    public:
        static constexpr float DefaultFadeSeconds = 0.15f;

        /// <summary>
        /// Switch to a clip. Asking for the clip already playing only updates its speed and
        /// looping, unless restart is set (e.g. a second hit reaction).
        /// </summary>
        void Play(Systems::Rendering::AnimationId clip, bool loop, float speed = 1.0f, bool restart = false,
                  float fadeSeconds = DefaultFadeSeconds) {
            if (clip == state_.current.clip && !restart) {
                state_.current.loop = loop;
                speed_ = speed;
                return;
            }

            if (state_.current.clip != Systems::Rendering::InvalidAnimationId && fadeSeconds > 0.0f) {
                state_.previous = state_.current;
                previousSpeed_ = speed_;
                state_.blend = 0.0f;
                fadeSeconds_ = fadeSeconds;
            }
            else {
                state_.previous = {};
                state_.blend = 1.0f;
            }

            state_.current = { clip, 0.0f, loop };
            speed_ = speed;
        }

        /// <summary>
        /// Advance the clips and the cross-fade
        /// </summary>
        void Update(float elapsedSeconds) {
            state_.current.timeSeconds += elapsedSeconds * speed_;
            if (state_.blend < 1.0f) {
                state_.previous.timeSeconds += elapsedSeconds * previousSpeed_;
                state_.blend = std::min(1.0f, state_.blend + elapsedSeconds / fadeSeconds_);
                if (state_.blend >= 1.0f) state_.previous = {};
            }
        }

        [[nodiscard]] const Systems::Rendering::AnimationState& GetState() const noexcept { return state_; }
        [[nodiscard]] Systems::Rendering::AnimationId CurrentClip() const noexcept { return state_.current.clip; }
        [[nodiscard]] float CurrentTime() const noexcept { return state_.current.timeSeconds; }

    private:
        Systems::Rendering::AnimationState state_{};
        float speed_{1.0f};
        float previousSpeed_{1.0f};
        float fadeSeconds_{DefaultFadeSeconds};
    };

} // namespace LB::Engine
