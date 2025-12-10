#include "npc_types.hpp"
#include "combat_visitor.hpp"

// ---------------- ORC ----------------

Orc::Orc(const std::string& name, double x, double y)
    : NPCBase(name, x, y) {}

int Orc::moveDistance() const { return 20; }
int Orc::killDistance() const { return 10; }

bool Orc::canKill(const NPCBase& other) const {
    return dynamic_cast<const Orc*>(&other) || dynamic_cast<const Bear*>(&other) || dynamic_cast<const Bear*>(&other);
}

std::string Orc::type() const { return "Orc"; }

void Orc::accept(CombatVisitor &v) { v.visit(*this); }

// ---------------- BEAR ----------------

Bear::Bear(const std::string& name, double x, double y)
    : NPCBase(name, x, y) {}

int Bear::moveDistance() const { return 5; }
int Bear::killDistance() const { return 10; }

bool Bear::canKill(const NPCBase& other) const {
    return dynamic_cast<const Squirrel*>(&other);
}

std::string Bear::type() const { return "Bear"; }

void Bear::accept(CombatVisitor &v) { v.visit(*this); }

// ---------------- SQUIRREL ----------------

Squirrel::Squirrel(const std::string& name, double x, double y)
    : NPCBase(name, x, y) {}

int Squirrel::moveDistance() const { return 5; }
int Squirrel::killDistance() const { return 5; }

bool Squirrel::canKill(const NPCBase&) const {
    return false;
}

std::string Squirrel::type() const { return "Squirrel"; }

void Squirrel::accept(CombatVisitor &v) { v.visit(*this); }

// ---------------- BANDIT ----------------

Bandit::Bandit(const std::string& name, double x, double y)
    : NPCBase(name, x, y) {}

int Bandit::moveDistance() const { return 10; }
int Bandit::killDistance() const { return 10; }

bool Bandit::canKill(const NPCBase& other) const {
    return dynamic_cast<const Werewolf*>(&other) == nullptr;
}

std::string Bandit::type() const { return "Bandit"; }

void Bandit::accept(CombatVisitor &v) { v.visit(*this); }

// ---------------- WEREWOLF ----------------

Werewolf::Werewolf(const std::string& name, double x, double y)
    : NPCBase(name, x, y) {}

int Werewolf::moveDistance() const { return 40; }
int Werewolf::killDistance() const { return 5; }

bool Werewolf::canKill(const NPCBase& other) const {
    return dynamic_cast<const Bandit*>(&other) != nullptr;
}

std::string Werewolf::type() const { return "Werewolf"; }

void Werewolf::accept(CombatVisitor &v) { v.visit(*this); }