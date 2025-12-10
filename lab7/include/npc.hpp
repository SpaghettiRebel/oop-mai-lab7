#pragma once
#include <string>
#include "combat_visitor.hpp"

class NPCBase {
protected:
    std::string name_;
    double x_;
    double y_;
    bool alive_{true};

public:
    NPCBase(const std::string& name, double x, double y)
        : name_(name), x_(x), y_(y) {}
    virtual ~NPCBase() = default;

    const std::string& name() const { return name_; }
    double x() const { return x_; }
    double y() const { return y_; }
    bool alive() const { return alive_; }
    void markDead() { alive_ = false; }
    void setPosition(double nx, double ny) { x_ = nx; y_ = ny; }

    virtual int moveDistance() const = 0;
    virtual int killDistance() const = 0;
    virtual bool canKill(const NPCBase& other) const = 0;
    virtual std::string type() const = 0;

    virtual void accept(CombatVisitor &v) = 0;
};
