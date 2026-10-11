// Sounds.ixx - The game's sounds: what plays for each moment (clip variants, volume), picking a
// variant without repeating the last one, and how loud and where a sound in the world is heard
export module LB.Engine:Sounds;

import std;
import LB.AudioSystem;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// A moment that makes a sound
    /// </summary>
    enum class GameSound : std::uint8_t {
        Footstep,       // the player walking on stone
        Swing,          // the player's weapon swinging
        EnemySwing,     // an enemy winding up an attack
        HitEnemy,       // a blow landing on a skeleton
        CriticalHit,    // ... a critical one
        PlayerHurt,     // an enemy's blow landing on the player
        EnemyDies,      // a skeleton falling apart
        EnemyRises,     // a skeleton climbing out of the ground
        Gold,           // picking up gold
        ItemPickup,     // picking up an item
        BagFull,        // an item left on the floor: no room
        ChestOpen,
        Descend,        // the trapdoor to the next floor
        Potion,         // drinking a potion
        LevelUp,
        ScreenOpen,     // the character screen
        ScreenClose,
        MenuMove,
        MenuConfirm,
        Equip,
        Shoot,          // a bow or crossbow loosing (the player's or an enemy's)
        ArrowHitsWall,  // an arrow or bolt striking stone
        Count
    };

    /// <summary>
    /// How a moment sounds: its clips (one is picked each time; files in the sound directory,
    /// .ogg), its volume, and whether it is interface feedback (heard during the pause, not placed
    /// in the world)
    /// </summary>
    struct SoundRecipe {
        std::span<const std::string_view> clips;
        float volume{1.0f};
        bool interface{false};
    };

    /// <summary>
    /// The sounds of the game (Kenney Impact Sounds and RPG Audio, CC0): their recipes, the clips
    /// to load, and a varied pick each time a sound plays
    /// </summary>
    class SoundBank {
    public:
        // A sound in the world is at full volume this close to the player (tiles), silent at
        // FarDistance, and panned fully to a side PanDistance to that side of the screen
        static constexpr float NearDistance = 3.0f;
        static constexpr float FarDistance = 18.0f;
        static constexpr float PanDistance = 9.0f;
        // A footstep every this many tiles walked
        static constexpr float StrideTiles = 1.6f;

        [[nodiscard]] static SoundRecipe RecipeFor(GameSound sound) noexcept;

        /// <summary>
        /// Every clip file the recipes use (without .ogg), each once
        /// </summary>
        [[nodiscard]] static std::vector<std::string_view> AllClips() {
            std::vector<std::string_view> all;
            for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(GameSound::Count); ++i) {
                for (const auto clip : RecipeFor(static_cast<GameSound>(i)).clips) {
                    if (std::ranges::find(all, clip) == all.end()) all.push_back(clip);
                }
            }
            return all;
        }

        /// <summary>
        /// The clip to play this time: a random variant, never the one played last for that sound
        /// (when there is more than one)
        /// </summary>
        [[nodiscard]] std::string_view PickClip(GameSound sound) {
            const auto clips = RecipeFor(sound).clips;
            if (clips.empty()) return {};
            auto& last = last_[static_cast<std::size_t>(sound)];
            std::size_t pick = 0;
            if (clips.size() > 1) {
                pick = std::uniform_int_distribution<std::size_t>(0, clips.size() - 2)(rng_);
                if (pick >= last) ++pick;   // skip the last one
            }
            last = pick;
            return clips[pick];
        }

        /// <summary>
        /// How a sound at a world point is heard from the player: volume falling off from
        /// NearDistance to FarDistance, and pan by how far to the screen's left or right it is
        /// (`screenRight`: the ground direction of screen right)
        /// </summary>
        [[nodiscard]] static std::pair<float, float> Placement(WorldPoint at, WorldPoint player, WorldPoint screenRight) noexcept {
            const float distance = Distance(at, player);
            const float volume = std::clamp(1.0f - (distance - NearDistance) / (FarDistance - NearDistance), 0.0f, 1.0f);
            const float side = (at.x - player.x) * screenRight.x + (at.y - player.y) * screenRight.y;
            return { volume, std::clamp(side / PanDistance, -1.0f, 1.0f) };
        }

    private:
        std::mt19937 rng_{ 4242u };
        std::array<std::size_t, static_cast<std::size_t>(GameSound::Count)> last_{};
    };

    // ------------------------------------------------------------------------------------------

    namespace SoundClips {
        inline constexpr std::string_view Footstep[] = { "footstep_concrete_000", "footstep_concrete_001", "footstep_concrete_002", "footstep_concrete_003", "footstep_concrete_004" };
        inline constexpr std::string_view Swing[] = { "knifeSlice", "knifeSlice2" };
        inline constexpr std::string_view EnemySwing[] = { "drawKnife1", "drawKnife2", "drawKnife3" };
        inline constexpr std::string_view HitEnemy[] = { "impactPlate_light_000", "impactPlate_light_001", "impactPlate_light_002", "impactPlate_light_003", "impactPlate_light_004" };
        inline constexpr std::string_view CriticalHit[] = { "impactMetal_heavy_000", "impactMetal_heavy_001", "impactMetal_heavy_002", "impactMetal_heavy_003", "impactMetal_heavy_004" };
        inline constexpr std::string_view PlayerHurt[] = { "impactPunch_medium_000", "impactPunch_medium_001", "impactPunch_medium_002", "impactPunch_medium_003", "impactPunch_medium_004" };
        inline constexpr std::string_view EnemyDies[] = { "impactWood_heavy_000", "impactWood_heavy_001", "impactWood_heavy_002", "impactWood_heavy_003", "impactWood_heavy_004" };
        inline constexpr std::string_view EnemyRises[] = { "impactMining_000", "impactMining_001", "impactMining_002", "impactMining_003", "impactMining_004" };
        inline constexpr std::string_view Gold[] = { "handleCoins", "handleCoins2" };
        inline constexpr std::string_view ItemPickup[] = { "handleSmallLeather", "handleSmallLeather2" };
        inline constexpr std::string_view BagFull[] = { "metalClick" };
        inline constexpr std::string_view ChestOpen[] = { "creak1", "creak2", "creak3" };
        inline constexpr std::string_view Descend[] = { "doorOpen_1", "doorOpen_2" };
        inline constexpr std::string_view Potion[] = { "metalPot1", "metalPot2", "metalPot3" };
        inline constexpr std::string_view LevelUp[] = { "impactBell_heavy_000", "impactBell_heavy_001" };
        inline constexpr std::string_view ScreenOpen[] = { "bookOpen" };
        inline constexpr std::string_view ScreenClose[] = { "bookClose" };
        inline constexpr std::string_view MenuMove[] = { "bookFlip1", "bookFlip2", "bookFlip3" };
        inline constexpr std::string_view MenuConfirm[] = { "bookPlace1" };
        inline constexpr std::string_view Equip[] = { "beltHandle1", "beltHandle2" };
        inline constexpr std::string_view Shoot[] = { "clothBelt", "clothBelt2" };
        inline constexpr std::string_view ArrowHitsWall[] = { "impactWood_light_000", "impactWood_light_001", "impactWood_light_002", "impactWood_light_003", "impactWood_light_004" };
    }

    inline SoundRecipe SoundBank::RecipeFor(GameSound sound) noexcept {
        using namespace SoundClips;
        switch (sound) {
        case GameSound::Footstep: return { Footstep, 0.35f };
        case GameSound::Swing: return { Swing, 0.6f };
        case GameSound::EnemySwing: return { EnemySwing, 0.45f };
        case GameSound::HitEnemy: return { HitEnemy, 0.8f };
        case GameSound::CriticalHit: return { CriticalHit, 0.9f };
        case GameSound::PlayerHurt: return { PlayerHurt, 0.9f };
        case GameSound::EnemyDies: return { EnemyDies, 0.8f };
        case GameSound::EnemyRises: return { EnemyRises, 0.6f };
        case GameSound::Gold: return { Gold, 0.7f };
        case GameSound::ItemPickup: return { ItemPickup, 0.7f };
        case GameSound::BagFull: return { BagFull, 0.7f };
        case GameSound::ChestOpen: return { ChestOpen, 0.8f };
        case GameSound::Descend: return { Descend, 0.8f };
        case GameSound::Potion: return { Potion, 0.7f };
        case GameSound::LevelUp: return { LevelUp, 0.6f };
        case GameSound::ScreenOpen: return { ScreenOpen, 0.6f, true };
        case GameSound::ScreenClose: return { ScreenClose, 0.6f, true };
        case GameSound::MenuMove: return { MenuMove, 0.4f, true };
        case GameSound::MenuConfirm: return { MenuConfirm, 0.6f, true };
        case GameSound::Equip: return { Equip, 0.7f, true };
        case GameSound::Shoot: return { Shoot, 0.8f };
        case GameSound::ArrowHitsWall: return { ArrowHitsWall, 0.6f };
        default: return {};
        }
    }

} // namespace LB::Engine
