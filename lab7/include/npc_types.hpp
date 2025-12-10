#pragma once
#include "npc.hpp"

class CombatVisitor;

class Orc final : public NPCBase {
public:
    Orc(const std::string& name, double x, double y);

    int moveDistance() const override;
    int killDistance() const override;
    bool canKill(const NPCBase& other) const override;
    std::string type() const override;

    void accept(CombatVisitor &v) override;
};

class Bear final : public NPCBase {
public:
    Bear(const std::string& name, double x, double y);

    int moveDistance() const override;
    int killDistance() const override;
    bool canKill(const NPCBase& other) const override;
    std::string type() const override;

    void accept(CombatVisitor &v) override;
};

class Squirrel final : public NPCBase {
public:
    Squirrel(const std::string& name, double x, double y);

    int moveDistance() const override;
    int killDistance() const override;
    bool canKill(const NPCBase& other) const override;
    std::string type() const override;

    void accept(CombatVisitor &v) override;
};

class Bandit final : public NPCBase {
public:
    Bandit(const std::string& name, double x, double y);

    int moveDistance() const override;
    int killDistance() const override;
    bool canKill(const NPCBase& other) const override;
    std::string type() const override;

    void accept(CombatVisitor &v) override;
};

class Werewolf final : public NPCBase {
public:
    Werewolf(const std::string& name, double x, double y);

    int moveDistance() const override;
    int killDistance() const override;
    bool canKill(const NPCBase& other) const override;
    std::string type() const override;

    void accept(CombatVisitor &v) override;
};
