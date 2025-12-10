#include <iostream>
#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <string>

#include "dungeon.hpp"
#include "factory.hpp"
#include "observer.hpp"
#include "npc.hpp"

// Потокобезопасный логгер в консоль
struct ConsoleLogger : public IObserver {
    explicit ConsoleLogger(std::mutex &m) : mtx(m) {}
    void onDeath(const DeathEvent &ev) override {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "[LOG] " << ev.killer << " убил " << ev.victim
                  << " в точке (" << ev.x << "," << ev.y << ")\n";
    }
private:
    std::mutex &mtx;
};

// Потокобезопасный логгер в файл
struct FileLogger : public IObserver {
    explicit FileLogger(const std::string &filename, std::mutex &m) : fn(filename), mtx(m) {}
    void onDeath(const DeathEvent &ev) override {
        std::lock_guard<std::mutex> lock(mtx);
        std::ofstream f(fn, std::ios::app);
        if(f) {
            f << ev.killer << " убил " << ev.victim
              << " в точке (" << ev.x << "," << ev.y << ")\n";
        }
    }
private:
    std::string fn;
    std::mutex &mtx;
};

int main() {
    Dungeon dungeon;

    std::mutex logMutex;

    // Подписываем логгеры
    dungeon.events().subscribe(std::make_shared<ConsoleLogger>(logMutex));
    dungeon.events().subscribe(std::make_shared<FileLogger>("log.txt", logMutex));

    // Генерация 50 NPC
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> xd(0.0, 100.0);
    std::uniform_real_distribution<double> yd(0.0, 100.0);
    std::vector<std::string> types = {"Orc", "Bear", "Squirrel", "Bandit", "Werewolf"};
    std::uniform_int_distribution<int> tid(0, (int)types.size() - 1);

    for(int i = 0; i < 50; ++i) {
        std::string t = types[tid(rng)];
        std::string name = t + "_" + std::to_string(i);
        auto up = NPCFactory::create(t, name, xd(rng), yd(rng));
        if(up) dungeon.addNPC(std::move(up));
    }

    // Запуск симуляции на 30 секунд
    dungeon.startSimulation(30);

    // Основной поток выводит карту каждую секунду
    for(int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        dungeon.printAll();
    }

    // Ждём завершения потоков
    dungeon.joinSimulation();

    // Финальный список выживших
    std::cout << "\n=== Survivors ===\n";
    dungeon.printAll();

    return 0;
}
