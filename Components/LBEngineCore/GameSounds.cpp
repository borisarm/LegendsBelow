// GameSounds.cpp - LB.Engine implementation unit: loading the game's sounds and playing them for
// what happens (GameEngine members; the recipes are in :Sounds). World sounds are heard from the
// player: quieter with distance and panned by screen side. Interface sounds are not placed and
// keep playing during the pause.
module LB.Engine;

import std;
import LB.AudioSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    // Every clip the recipes use, from the sound directory (<name>.ogg). Without a directory the
    // game is silent; a missing or invalid clip stops startup with every problem listed.
    bool GameEngine::LoadSounds(const std::string& soundDirectory) {
        assets_.soundsLoaded = false;
        if (soundDirectory.empty()) return true;

        const auto directory = std::filesystem::path(std::u8string(soundDirectory.begin(), soundDirectory.end()));
        std::vector<std::string> errors;
        for (const auto clip : SoundBank::AllClips()) {
            const auto result = audioSystem_.LoadClip(std::string(clip), directory / (std::string(clip) + ".ogg"));
            if (!result.Succeeded()) errors.push_back(result.error);
        }
        for (const auto clip : Ambience::Clips) {
            const auto result = audioSystem_.LoadClip(std::string(clip), directory / (std::string(clip) + ".wav"));
            if (!result.Succeeded()) errors.push_back(result.error);
        }
        if (!errors.empty()) {
            initializationError_ = std::format("Sounds in '{}' could not be loaded ({} problems):", soundDirectory, errors.size());
            for (const auto& error : errors) initializationError_ += "\n" + error;
            return false;
        }
        assets_.soundsLoaded = true;
        return true;
    }

    // The music tracks from the music directory; without a directory there is no music, a
    // missing or invalid track stops startup
    bool GameEngine::LoadMusic(const std::string& musicDirectory) {
        assets_.musicLoaded = false;
        if (musicDirectory.empty()) return true;

        const auto directory = std::filesystem::path(std::u8string(musicDirectory.begin(), musicDirectory.end()));
        std::vector<std::string> errors;
        for (const auto track : MusicTracks::All) {
            const auto result = audioSystem_.LoadClip(std::string(track), directory / track);
            if (!result.Succeeded()) errors.push_back(result.error);
        }
        if (!errors.empty()) {
            initializationError_ = std::format("Music in '{}' could not be loaded ({} problems):", musicDirectory, errors.size());
            for (const auto& error : errors) initializationError_ += "\n" + error;
            return false;
        }
        assets_.musicLoaded = true;
        return true;
    }

    // Both layers loop from the start, silent until the director brings them in: the floor's (or
    // the arena's) calm theme and the combat theme
    void GameEngine::StartMusic() {
        if (!assets_.musicLoaded) return;
        using Systems::Audio::AudioChannel;
        const auto calm = std::string(session_->dungeonMode ? MusicTracks::DungeonCalm : MusicTracks::ArenaCalm);
        session_->calmMusic = audioSystem_.PlaySound(calm, AudioChannel::Music, 0.0f, true);
        session_->combatMusic = audioSystem_.PlaySound(std::string(MusicTracks::Combat), AudioChannel::Music, 0.0f, true);
    }

    // A fight is on while any enemy is after the player (chasing, attacking, or reeling from a hit)
    void GameEngine::UpdateMusic(float elapsedSeconds) {
        if (!assets_.musicLoaded) return;
        bool fighting = false;
        if (IsPlayerAlive()) {
            for (const auto& enemy : session_->encounter.GetEnemies()) {
                if (enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking || enemy.state == EnemyState::Staggered) {
                    fighting = true;
                    break;
                }
            }
        }
        session_->music.Update(elapsedSeconds, fighting);
        audioSystem_.SetVolume(session_->calmMusic, session_->music.CalmVolume());
        audioSystem_.SetVolume(session_->combatMusic, session_->music.CombatVolume());
    }

    void GameEngine::PlaySound(GameSound sound, std::optional<WorldPoint> at) {
        if (!assets_.soundsLoaded) return;
        const auto recipe = SoundBank::RecipeFor(sound);
        const auto clip = std::string(session_->sounds.PickClip(sound));
        if (clip.empty()) return;

        using Systems::Audio::AudioChannel;
        if (recipe.interface) {
            audioSystem_.PlaySound(clip, AudioChannel::Master, recipe.volume);
            return;
        }
        float volume = recipe.volume;
        float pan = 0.0f;
        if (at) {
            const auto right = session_->camera.ScreenToGround(1.0f, 0.0f);
            const auto [heard, side] = SoundBank::Placement(*at, session_->playerPosition, { right.x, right.y });
            volume *= heard;
            pan = side;
        }
        if (volume <= 0.01f) return;   // too far to hear
        audioSystem_.PlaySound(clip, AudioChannel::SoundEffects, volume, false, pan);
    }

    // This step's blows: a clatter where they landed (a heavier one for criticals), bones falling
    // apart for kills, and a thud when the player is hit
    void GameEngine::PlayCombatSounds() {
        for (const auto& event : session_->combatEvents) {
            if (!event.result.hit || event.result.finalDamage <= 0) continue;
            if (event.kind == CombatEventKind::PlayerAttack) {
                const auto* enemy = session_->encounter.FindEnemy(event.enemyId);
                if (!enemy) continue;
                PlaySound(event.result.critical ? GameSound::CriticalHit : GameSound::HitEnemy, enemy->position);
                if (event.killed) PlaySound(GameSound::EnemyDies, enemy->position);
            }
            else {
                PlaySound(GameSound::PlayerHurt, session_->playerPosition);
            }
        }
    }

    // This step's spells: bursts boom (fire) or shatter, missiles ring where they land
    void GameEngine::PlaySpellSounds() {
        for (const auto id : session_->encounter.GetPhaseChanges()) {
            if (const auto* boss = session_->encounter.FindEnemy(id)) PlaySound(GameSound::LevelUp, boss->position);   // a toll: it grows stronger
        }
        for (const auto& burst : session_->encounter.GetBursts()) {
            PlaySound(burst.damageType == LB::Combat::DamageType::Fire ? GameSound::FireBurst : GameSound::FrostBurst, burst.at);
        }
        for (const auto& event : session_->combatEvents) {
            if (!event.spell || event.spell->radius > 0.0f) continue;
            const auto* enemy = event.kind == CombatEventKind::PlayerAttack ? session_->encounter.FindEnemy(event.enemyId) : nullptr;
            PlaySound(GameSound::SpellHit, enemy ? enemy->position : session_->playerPosition);
        }
    }

    // This step's shots: enemies loosing (the player's own are heard as they leave), and arrows and
    // bolts striking walls
    void GameEngine::PlayProjectileSounds() {
        for (const auto at : session_->encounter.GetShots()) PlaySound(GameSound::Shoot, at);   // casters' spells too
        for (const auto at : session_->encounter.GetWallHits()) PlaySound(GameSound::ArrowHitsWall, at);
    }

    // Each step: advance playback; while the game runs, footsteps as the player walks, and the
    // sounds of enemies starting to rise or to swing (aiming is quiet: the shot is heard)
    void GameEngine::UpdateSounds(float elapsedSeconds, bool running) {
        audioSystem_.Update(elapsedSeconds);
        UpdateMusic(elapsedSeconds);
        UpdateAmbience();
        if (!running) {
            session_->soundPlayerPosition = session_->playerPosition;
            return;
        }

        // A footstep every stride; jumps (respawn, a new floor, a load) are not walked
        const float moved = Distance(session_->playerPosition, session_->soundPlayerPosition);
        session_->soundPlayerPosition = session_->playerPosition;
        if (IsPlayerAlive() && moved < 1.0f) {
            session_->strideTiles += moved;
            if (session_->strideTiles >= SoundBank::StrideTiles) {
                session_->strideTiles -= SoundBank::StrideTiles;
                PlaySound(Ambience::FootstepsFor(GetTheme().name), session_->playerPosition);
            }
        }

        // Enemies entering a state that makes a sound
        auto& heard = session_->enemyStatesHeard;
        std::unordered_map<std::uint32_t, EnemyState> now;
        for (const auto& enemy : session_->encounter.GetEnemies()) {
            const auto before = heard.find(enemy.id);
            const bool entered = before == heard.end() || before->second != enemy.state;
            if (entered && enemy.state == EnemyState::Spawning) PlaySound(GameSound::EnemyRises, enemy.position);
            if (entered && enemy.state == EnemyState::Attacking && !enemy.definition->IsRanged()) PlaySound(GameSound::EnemySwing, enemy.position);
            now.emplace(enemy.id, enemy.state);
        }
        heard = std::move(now);
    }

    // The floor's ambience loops quietly under everything (none in the arena); a new tier swaps it
    void GameEngine::UpdateAmbience() {
        if (!assets_.soundsLoaded) return;
        const std::string_view wanted = session_->dungeonMode ? Ambience::For(GetTheme().name) : std::string_view{};
        if (wanted == session_->ambienceClip) return;
        if (session_->ambienceClip.size() > 0) audioSystem_.Stop(session_->ambienceSource);
        session_->ambienceClip = std::string(wanted);
        if (!wanted.empty()) {
            session_->ambienceSource = audioSystem_.PlaySound(std::string(wanted), Systems::Audio::AudioChannel::SoundEffects, Ambience::Volume, true);
        }
    }

} // namespace LB::Engine
