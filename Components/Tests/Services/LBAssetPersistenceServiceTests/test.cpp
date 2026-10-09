#include "pch.h"
#include "RepositoryTestHelpers.h"

using namespace LB::Testing::Repositories;
using LB::Repositories::SpellRepository;
using LB::Repositories::Factory::MakeSharedSpellRepository;
using LB::Repositories::Factory::MakeSpellRepository;
using LB::Repositories::Factory::MakeSpellRepositoryWithAllocator;

namespace {
    // Order-independent comparison of ID lists
    std::vector<SpellId> Sorted(std::vector<SpellId> ids) {
        std::ranges::sort(ids);
        return ids;
    }

    SpellRepository MakeSample() {
        return MakeSpellRepository(CreateSampleRepository());
    }
}

// ============================================================================
// Factory Tests
// ============================================================================

TEST(SpellRepositoryFactoryTests, MakeSpellRepository_ReturnsUsableProxy) {
    SpellRepository repo = MakeSample();

    ASSERT_TRUE(repo.has_value());
    EXPECT_EQ(repo->GetAllSpellIds().size(), 4u);
}

TEST(SpellRepositoryFactoryTests, MakeSpellRepository_EmptyImplementation_NoSpells) {
    SpellRepository repo = MakeSpellRepository(MockSpellRepository{});

    EXPECT_TRUE(repo->GetAllSpellIds().empty());
}

TEST(SpellRepositoryFactoryTests, MakeSharedSpellRepository_ReturnsUsableProxy) {
    SpellRepository repo = MakeSharedSpellRepository(CreateSampleRepository());

    ASSERT_TRUE(repo.has_value());
    EXPECT_TRUE(repo->SpellExists(TestSpells::Fireball));
}

TEST(SpellRepositoryFactoryTests, MakeSpellRepositoryWithAllocator_UsesAllocator) {
    CountingAllocator<std::byte> allocator;

    SpellRepository repo = MakeSpellRepositoryWithAllocator(allocator, CreateSampleRepository());

    ASSERT_TRUE(repo.has_value());
    EXPECT_GE(*allocator.allocations, 1u);
    EXPECT_TRUE(repo->SpellExists(TestSpells::MagicMissile));
}

TEST(SpellRepositoryFactoryTests, Copy_ProducesIndependentProxyWithSameData) {
    SpellRepository original = MakeSample();

    SpellRepository copy = original;

    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(Sorted(copy->GetAllSpellIds()), Sorted(original->GetAllSpellIds()));
}

// ============================================================================
// Lookup Tests
// ============================================================================

TEST(SpellRepositoryTests, GetSpell_ExistingId_ReturnsSpell) {
    SpellRepository repo = MakeSample();

    auto spell = repo->GetSpell(TestSpells::Fireball);

    ASSERT_TRUE(spell.has_value());
    EXPECT_EQ(spell->id, TestSpells::Fireball);
    EXPECT_EQ(spell->name, "Fireball");
    EXPECT_EQ(spell->manaCost, 30);
    EXPECT_EQ(spell->baseDamage, 28);
}

TEST(SpellRepositoryTests, GetSpell_UnknownId_ReturnsNullopt) {
    SpellRepository repo = MakeSample();

    EXPECT_FALSE(repo->GetSpell(999).has_value());
}

TEST(SpellRepositoryTests, GetSpell_InvalidId_ReturnsNullopt) {
    SpellRepository repo = MakeSample();

    EXPECT_FALSE(repo->GetSpell(LB::InvalidId).has_value());
}

TEST(SpellRepositoryTests, SpellExists_KnownAndUnknown) {
    SpellRepository repo = MakeSample();

    EXPECT_TRUE(repo->SpellExists(TestSpells::DivineLight));
    EXPECT_FALSE(repo->SpellExists(999));
}

TEST(SpellRepositoryTests, GetAllSpellIds_ReturnsEveryId) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetAllSpellIds()),
        (std::vector<SpellId>{ TestSpells::MagicMissile, TestSpells::Fireball, TestSpells::FrostNova, TestSpells::DivineLight }));
}

// ============================================================================
// Filter Tests
// ============================================================================

TEST(SpellRepositoryTests, GetSpellsBySchool_Evocation_ReturnsOnlyEvocation) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetSpellsBySchool(SpellSchool::Evocation)),
        (std::vector<SpellId>{ TestSpells::MagicMissile, TestSpells::Fireball }));
}

TEST(SpellRepositoryTests, GetSpellsBySchool_NoMatches_ReturnsEmpty) {
    SpellRepository repo = MakeSample();

    EXPECT_TRUE(repo->GetSpellsBySchool(SpellSchool::Shadow).empty());
}

TEST(SpellRepositoryTests, GetSchoolDescription_KnownSchool_ReturnsText) {
    SpellRepository repo = MakeSample();

    auto description = repo->GetSchoolDescription(SpellSchool::Frost);

    ASSERT_TRUE(description.has_value());
    EXPECT_FALSE(description->empty());
}

TEST(SpellRepositoryTests, GetSchoolDescription_None_ReturnsNullopt) {
    SpellRepository repo = MakeSample();

    EXPECT_FALSE(repo->GetSchoolDescription(SpellSchool::None).has_value());
}

TEST(SpellRepositoryTests, GetSpellsByClass_Cleric_ReturnsClericSpells) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(repo->GetSpellsByClass(CharacterClass::Cleric), (std::vector<SpellId>{ TestSpells::DivineLight }));
}

TEST(SpellRepositoryTests, GetSpellsByClass_Warrior_ReturnsEmpty) {
    SpellRepository repo = MakeSample();

    EXPECT_TRUE(repo->GetSpellsByClass(CharacterClass::Warrior).empty());
}

TEST(SpellRepositoryTests, GetSpellsByLevel_LevelOne_ReturnsLevelOneSpells) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetSpellsByLevel(1)),
        (std::vector<SpellId>{ TestSpells::MagicMissile, TestSpells::DivineLight }));
}

TEST(SpellRepositoryTests, GetSpellsByCategory_Control_MatchesMultiCategorySpell) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(repo->GetSpellsByCategory(SpellCategory::Control), (std::vector<SpellId>{ TestSpells::FrostNova }));
}

TEST(SpellRepositoryTests, GetSpellsByCategory_Damage_ReturnsAllDamageSpells) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetSpellsByCategory(SpellCategory::Damage)),
        (std::vector<SpellId>{ TestSpells::MagicMissile, TestSpells::Fireball, TestSpells::FrostNova }));
}

TEST(SpellRepositoryTests, GetSpellsByManaCost_RangeIsInclusive) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetSpellsByManaCost(15, 20)),
        (std::vector<SpellId>{ TestSpells::FrostNova, TestSpells::DivineLight }));
}

TEST(SpellRepositoryTests, GetSpellsByManaCost_InvertedRange_ReturnsEmpty) {
    SpellRepository repo = MakeSample();

    EXPECT_TRUE(repo->GetSpellsByManaCost(50, 10).empty());
}

TEST(SpellRepositoryTests, GetSpellsByCastTime_IncludesInstantAndBoundary) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(Sorted(repo->GetSpellsByCastTime(1.0f)),
        (std::vector<SpellId>{ TestSpells::MagicMissile, TestSpells::FrostNova, TestSpells::DivineLight }));
}

TEST(SpellRepositoryTests, GetSpellsByCastTime_Zero_ReturnsOnlyInstant) {
    SpellRepository repo = MakeSample();

    EXPECT_EQ(repo->GetSpellsByCastTime(0.0f), (std::vector<SpellId>{ TestSpells::MagicMissile }));
}
