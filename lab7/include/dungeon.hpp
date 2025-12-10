#pragma once
#include <vector>
#include <memory>
#include <string>
#include <mutex>

class NPCBase;
class EventManager;

class Dungeon {
public:
    explicit Dungeon();
    ~Dungeon();

    bool addNPC(std::unique_ptr<NPCBase> npc);
    bool loadFromFile(const std::string &fname);
    bool saveToFile(const std::string &fname) const;
    void clear() noexcept;

    void printAll() const;

    EventManager& events() noexcept;

    void runCombat(double range);

    void startSimulation(int seconds);
    void stopSimulation();
    void joinSimulation();

    std::mutex & coutMutex() const noexcept;

private:
    struct Impl;
    Impl* pimpl_;
};
