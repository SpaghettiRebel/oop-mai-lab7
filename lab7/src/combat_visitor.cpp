#include "combat_visitor.hpp"
#include "npc.hpp"
#include "npc_types.hpp"
#include <string>

static bool wantsKill(const std::string &A, const std::string &B) noexcept {
    if (A == "Orc") return (B == "Bear" || B == "Orc" || B == "Bandit");
    if (A == "Bear") return (B == "Squirrel");
    if (A == "Squirrel") return false;
    if (A == "Bandit") return (B == "Werewolf");
    if (A == "Werewolf") return (B == "Bandit");
    return false;
}

CombatVisitor::CombatVisitor(NPCBase* attacker) noexcept 
    : attacker_(attacker), victimDies_(false), attackerDies_(false) {}

bool CombatVisitor::victimDies() const noexcept { return victimDies_; }
bool CombatVisitor::attackerDies() const noexcept { return attackerDies_; }

void CombatVisitor::visit(Orc &def) {
    const std::string A = attacker_->type();
    const std::string B = def.type();
    victimDies_ = wantsKill(A, B);
    attackerDies_ = wantsKill(B, A);
}

void CombatVisitor::visit(Bear &def) {
    const std::string A = attacker_->type();
    const std::string B = def.type();
    victimDies_ = wantsKill(A, B);
    attackerDies_ = wantsKill(B, A);
}

void CombatVisitor::visit(Squirrel &def) {
    const std::string A = attacker_->type();
    const std::string B = def.type();
    victimDies_ = wantsKill(A, B);
    attackerDies_ = wantsKill(B, A);
}

void CombatVisitor::visit(Bandit &def) {
    const std::string A = attacker_->type();
    const std::string B = def.type();
    victimDies_ = wantsKill(A, B);
    attackerDies_ = wantsKill(B, A);
}
void CombatVisitor::visit(Werewolf &def) {
    const std::string A = attacker_->type();
    const std::string B = def.type();
    victimDies_ = wantsKill(A, B);
    attackerDies_ = wantsKill(B, A);
}