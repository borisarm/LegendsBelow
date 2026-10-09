// StepTimer.ixx - Fixed/variable timestep frame timer
//
// Ported from the DirectX template's StepTimer, using std::chrono::steady_clock instead of
// QueryPerformanceCounter so the engine has no Win32 dependency.
export module LB.Engine:StepTimer;

import std;

export namespace LB::Engine {

    /// <summary>
    /// Helper class for animation and simulation timing.
    /// In fixed timestep mode the update callback runs zero or more times per tick so the
    /// simulation always advances in exact TargetElapsed increments.
    /// </summary>
    class StepTimer {
    public:
        using Clock = std::chrono::steady_clock;

        /// <summary>
        /// Integer format represents time using 10,000,000 ticks per second.
        /// </summary>
        static constexpr std::uint64_t TicksPerSecond = 10'000'000;

        using Ticks = std::chrono::duration<std::int64_t, std::ratio<1, TicksPerSecond>>;

        static constexpr double TicksToSeconds(std::uint64_t ticks) noexcept {
            return static_cast<double>(ticks) / TicksPerSecond;
        }

        static constexpr std::uint64_t SecondsToTicks(double seconds) noexcept {
            return static_cast<std::uint64_t>(seconds * TicksPerSecond);
        }

        StepTimer() : lastTime_(Clock::now()) {}

        // Get elapsed time since the previous Update call.
        std::uint64_t GetElapsedTicks() const noexcept { return elapsedTicks_; }
        double GetElapsedSeconds() const noexcept { return TicksToSeconds(elapsedTicks_); }

        // Get total simulated time since the timer started.
        std::uint64_t GetTotalTicks() const noexcept { return totalTicks_; }
        double GetTotalSeconds() const noexcept { return TicksToSeconds(totalTicks_); }

        // Get total number of updates since the timer started.
        std::uint32_t GetFrameCount() const noexcept { return frameCount_; }

        // Get the current framerate.
        std::uint32_t GetFramesPerSecond() const noexcept { return framesPerSecond_; }

        // Set whether to use fixed or variable timestep mode.
        bool IsFixedTimeStep() const noexcept { return isFixedTimeStep_; }
        void SetFixedTimeStep(bool isFixedTimestep) noexcept { isFixedTimeStep_ = isFixedTimestep; }

        // Set how often to call Update when in fixed timestep mode.
        std::uint64_t GetTargetElapsedTicks() const noexcept { return targetElapsedTicks_; }
        void SetTargetElapsedTicks(std::uint64_t targetElapsed) noexcept { targetElapsedTicks_ = targetElapsed; }
        void SetTargetElapsedSeconds(double targetElapsed) noexcept { targetElapsedTicks_ = SecondsToTicks(targetElapsed); }

        /// <summary>
        /// After an intentional timing discontinuity (for instance a blocking IO operation)
        /// call this to avoid having the fixed timestep logic attempt a set of catch-up
        /// Update calls.
        /// </summary>
        void ResetElapsedTime() {
            lastTime_ = Clock::now();

            leftOverTicks_ = 0;
            framesPerSecond_ = 0;
            framesThisSecond_ = 0;
            secondCounter_ = 0;
        }

        /// <summary>
        /// Measure the real time since the previous tick and advance the timer,
        /// calling the update function the appropriate number of times.
        /// </summary>
        template<typename TUpdate>
        void Tick(const TUpdate& update) {
            const auto currentTime = Clock::now();
            const auto realDelta = currentTime - lastTime_;
            lastTime_ = currentTime;

            Advance(realDelta, update);
        }

        /// <summary>
        /// Advance the timer by an explicit amount of real time (deterministic stepping for tests and replays).
        /// </summary>
        template<typename TUpdate>
        void Advance(Clock::duration realDelta, const TUpdate& update) {
            const auto deltaTicks = std::chrono::duration_cast<Ticks>(realDelta).count();
            std::uint64_t timeDelta = deltaTicks > 0 ? static_cast<std::uint64_t>(deltaTicks) : 0;

            secondCounter_ += timeDelta;

            // Clamp excessively large time deltas (e.g. after paused in the debugger).
            if (timeDelta > MaxDeltaTicks) {
                timeDelta = MaxDeltaTicks;
            }

            const std::uint32_t lastFrameCount = frameCount_;

            if (isFixedTimeStep_) {
                // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
                // the clock to exactly match the target value. This prevents tiny and irrelevant errors
                // from accumulating over time. Without this clamping, a game that requested a 60 fps
                // fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
                // accumulate enough tiny errors that it would drop a frame. It is better to just round
                // small deviations down to zero to leave things running smoothly.
                if (static_cast<std::uint64_t>(std::abs(static_cast<std::int64_t>(timeDelta - targetElapsedTicks_))) < TicksPerSecond / 4000) {
                    timeDelta = targetElapsedTicks_;
                }

                leftOverTicks_ += timeDelta;

                while (leftOverTicks_ >= targetElapsedTicks_) {
                    elapsedTicks_ = targetElapsedTicks_;
                    totalTicks_ += targetElapsedTicks_;
                    leftOverTicks_ -= targetElapsedTicks_;
                    frameCount_++;

                    update();
                }
            }
            else {
                // Variable timestep update logic.
                elapsedTicks_ = timeDelta;
                totalTicks_ += timeDelta;
                leftOverTicks_ = 0;
                frameCount_++;

                update();
            }

            // Track the current framerate.
            if (frameCount_ != lastFrameCount) {
                framesThisSecond_++;
            }

            if (secondCounter_ >= TicksPerSecond) {
                framesPerSecond_ = framesThisSecond_;
                framesThisSecond_ = 0;
                secondCounter_ %= TicksPerSecond;
            }
        }

    private:
        // Maximum real time consumed by a single tick: 1/10 of a second.
        static constexpr std::uint64_t MaxDeltaTicks = TicksPerSecond / 10;

        Clock::time_point lastTime_;

        // Derived timing data uses a canonical tick format.
        std::uint64_t elapsedTicks_{0};
        std::uint64_t totalTicks_{0};
        std::uint64_t leftOverTicks_{0};

        // Members for tracking the framerate.
        std::uint32_t frameCount_{0};
        std::uint32_t framesPerSecond_{0};
        std::uint32_t framesThisSecond_{0};
        std::uint64_t secondCounter_{0};

        // Members for configuring fixed timestep mode.
        bool isFixedTimeStep_{false};
        std::uint64_t targetElapsedTicks_{TicksPerSecond / 60};
    };

} // namespace LB::Engine
