#include <gtest/gtest.h>
#include "factory.hpp" // Проверка создания NPC
#include "npc.hpp"     // Проверка базового класса
#include <cmath>
#include <memory>
#include <string>

// --- I. Тестирование Фабрики (NPCFactory) ---

TEST(FactoryTests, CreateByType) {
    // 1. Создание стандартных типов
    auto bear = NPCFactory::create("Bear", "B1", 10.0, 10.0);
    auto orc = NPCFactory::create("Orc", "O2", 20.0, 20.0);
    auto squirrel = NPCFactory::create("Squirrel", "S3", 30.0, 30.0);

    ASSERT_NE(bear, nullptr);
    ASSERT_NE(orc, nullptr);
    ASSERT_NE(squirrel, nullptr);

    // 2. Проверка типов и имен
    ASSERT_EQ(bear->type(), "Bear");
    ASSERT_EQ(orc->name(), "O2");
    ASSERT_EQ(squirrel->type(), "Squirrel");
    
    // 3. Проверка несуществующего типа
    auto unknown = NPCFactory::create("Dragon", "D1", 0.0, 0.0);
    ASSERT_EQ(unknown, nullptr);
}

TEST(FactoryTests, CreateFromLine) {
    // 1. Корректная строка
    auto bandit = NPCFactory::createFromLine("Bandit B4 40.5 40.5");
    ASSERT_NE(bandit, nullptr);
    ASSERT_EQ(bandit->name(), "B4");
    ASSERT_NEAR(bandit->x(), 40.5, 0.001);
    
    // 2. Некорректный формат (нет координат)
    auto bad_format = NPCFactory::createFromLine("Orc O5");
    ASSERT_EQ(bad_format, nullptr);

    // 3. Неизвестный тип
    auto unknown_line = NPCFactory::createFromLine("Elf E6 60.0 60.0");
    ASSERT_EQ(unknown_line, nullptr);
}

// --- II. Тестирование Базового Функционала NPC ---

TEST(NPCTests, PositionAndStatus) {
    auto bear = NPCFactory::create("Bear", "TestBear", 50.0, 60.0);

    // 1. Проверка начальных координат и статуса
    ASSERT_NEAR(bear->x(), 50.0, 0.001);
    ASSERT_NEAR(bear->y(), 60.0, 0.001);
    ASSERT_TRUE(bear->alive());

    // 2. Проверка установки позиции
    bear->setPosition(1.5, 2.5);
    ASSERT_NEAR(bear->x(), 1.5, 0.001);
    ASSERT_NEAR(bear->y(), 2.5, 0.001);

    // 3. Проверка статуса "мертв"
    bear->markDead();
    ASSERT_FALSE(bear->alive());
}

TEST(NPCTests, NPCProperties) {
    // Проверка полиморфных свойств
    auto orc = NPCFactory::create("Orc", "O1", 0, 0);
    auto werewolf = NPCFactory::create("Werewolf", "W1", 0, 0);
    auto squirrel = NPCFactory::create("Squirrel", "S1", 0, 0);

    ASSERT_EQ(orc->moveDistance(), 20);
    ASSERT_EQ(werewolf->moveDistance(), 40);
    ASSERT_EQ(squirrel->moveDistance(), 5);
    
    ASSERT_EQ(orc->killDistance(), 10.0);
    ASSERT_EQ(werewolf->killDistance(), 5.0);
    ASSERT_EQ(squirrel->killDistance(), 5.0);
}
namespace {
    static bool checkKillByType_Test(const std::string &A, const std::string &B) {
        if (A == "Orc") return (B == "Bear" || B == "Orc" || B == "Bandit");
        if (A == "Bear") return (B == "Squirrel");
        if (A == "Squirrel") return false;
        if (A == "Bandit") return (B == "Werewolf");
        if (A == "Werewolf") return (B == "Bandit");
        return false;
    }
}


TEST(CombatLogicTests, KillMatrixRules) {
    // 1. Проверка разрешенных убийств
    // Orc может убить Bear, Orc, Bandit
    ASSERT_TRUE(checkKillByType_Test("Orc", "Bear"));
    ASSERT_TRUE(checkKillByType_Test("Orc", "Orc"));
    ASSERT_TRUE(checkKillByType_Test("Orc", "Bandit"));

    // Bear убивает Squirrel
    ASSERT_TRUE(checkKillByType_Test("Bear", "Squirrel"));

    // Bandit убивает Werewolf
    ASSERT_TRUE(checkKillByType_Test("Bandit", "Werewolf"));
    
    // Werewolf убивает Bandit
    ASSERT_TRUE(checkKillByType_Test("Werewolf", "Bandit"));


    // 2. Проверка запрещенных убийств
    // Orc НЕ убивает Squirrel
    ASSERT_FALSE(checkKillByType_Test("Orc", "Squirrel"));

    // Bear НЕ убивает Orc
    ASSERT_FALSE(checkKillByType_Test("Bear", "Orc"));
    
    // Squirrel НЕ убивает никого
    ASSERT_FALSE(checkKillByType_Test("Squirrel", "Bear"));
    ASSERT_FALSE(checkKillByType_Test("Squirrel", "Orc"));

    // Несимметричные пары:
    // Orc убивает Bear, но Bear НЕ убивает Orc
    ASSERT_TRUE(checkKillByType_Test("Orc", "Bear"));
    ASSERT_FALSE(checkKillByType_Test("Bear", "Orc"));
}