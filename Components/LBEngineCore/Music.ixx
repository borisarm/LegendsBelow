// Music.ixx - The game's music: a calm layer and a combat layer playing together, crossfading
// with how the fight goes (decisions only; the engine plays the tracks)
export module LB.Engine:Music;

import std;

export namespace LB::Engine {

    /// <summary>
    /// The music tracks (files in the music directory) and which play where: the dungeon's calm
    /// theme, the wave arena's, and the combat theme both share
    /// </summary>
    struct MusicTracks {
        static constexpr std::string_view DungeonCalm = "mystical_theme.mp3";
        static constexpr std::string_view ArenaCalm = "awesomeness.wav";
        static constexpr std::string_view Combat = "battle_theme_a.mp3";
        static constexpr std::array<std::string_view, 3> All = { DungeonCalm, ArenaCalm, Combat };
    };

    /// <summary>
    /// Mixes the two layers: the calm layer while exploring, the combat layer from the moment an
    /// enemy is engaged until things have been quiet for CalmAfterSeconds. Both keep looping, so a
    /// layer comes back where it was. Volumes are the layers' share of the music channel.
    /// </summary>
    class MusicDirector {
    public:
        static constexpr float LayerVolume = 0.8f;        // a layer at full presence
        static constexpr float FadeInSeconds = 2.0f;      // the music starting
        static constexpr float CombatInSeconds = 1.5f;    // into a fight
        static constexpr float CombatOutSeconds = 3.0f;   // back to calm
        static constexpr float CalmAfterSeconds = 4.0f;   // quiet this long ends the fight

        /// <summary>
        /// Advance by a step: `fighting` while any enemy is engaged with the player
        /// </summary>
        void Update(float elapsedSeconds, bool fighting) {
            quietSeconds_ = fighting ? 0.0f : quietSeconds_ + elapsedSeconds;
            inCombat_ = fighting || (inCombat_ && quietSeconds_ < CalmAfterSeconds);

            const float calmGoal = inCombat_ ? 0.0f : LayerVolume;
            const float combatGoal = inCombat_ ? LayerVolume : 0.0f;
            // Fading in from silence at the start is gentler than the crossfades
            const float calmRate = LayerVolume / (started_ ? (inCombat_ ? CombatInSeconds : CombatOutSeconds) : FadeInSeconds);
            const float combatRate = LayerVolume / (inCombat_ ? CombatInSeconds : CombatOutSeconds);
            calm_ = Approach(calm_, calmGoal, calmRate * elapsedSeconds);
            combat_ = Approach(combat_, combatGoal, combatRate * elapsedSeconds);
            if (calm_ >= LayerVolume) started_ = true;
        }

        [[nodiscard]] float CalmVolume() const noexcept { return calm_; }
        [[nodiscard]] float CombatVolume() const noexcept { return combat_; }
        [[nodiscard]] bool InCombat() const noexcept { return inCombat_; }

    private:
        static float Approach(float value, float goal, float step) noexcept {
            return value < goal ? std::min(goal, value + step) : std::max(goal, value - step);
        }

        float calm_{0.0f};
        float combat_{0.0f};
        float quietSeconds_{0.0f};
        bool inCombat_{false};
        bool started_{false};
    };

} // namespace LB::Engine
