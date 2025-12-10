#pragma once
#include <string>

class NPCBase;
class Orc;
class Bear;
class Squirrel;
class Bandit;
class Werewolf;


class CombatVisitor {
public:
    explicit CombatVisitor(NPCBase* attacker) noexcept;

    bool victimDies() const noexcept;
    bool attackerDies() const noexcept;

    void visit(Orc &def);
    void visit(Bear &def);
    void visit(Squirrel &def);
    void visit(Bandit &def);
    void visit(Werewolf &def);

private:
    NPCBase* attacker_;
    bool victimDies_ = false;
    bool attackerDies_ = false;
};