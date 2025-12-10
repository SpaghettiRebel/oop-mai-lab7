#include "dungeon.hpp"
#include "factory.hpp"
#include "observer.hpp"
#include "combat_visitor.hpp"
#include "npc.hpp"

#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>
#include <deque>
#include <random>
#include <chrono>
#include <set>
#include <cmath>
#include <unordered_map>

struct Dungeon::Impl {
    std::vector<std::shared_ptr<NPCBase>> npcs; // shared_ptr for safe multi-thread access
    EventManager events;

    mutable std::shared_mutex npcs_mutex; // protect npcs
    std::mutex cout_mutex; // for external loggers (ConsoleLogger)
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::deque<std::pair<std::shared_ptr<NPCBase>, std::shared_ptr<NPCBase>>> fight_queue;
    std::atomic<bool> stop_flag{false};
    std::thread movement_thread;
    std::thread battle_thread;
};

Dungeon::Dungeon() : pimpl_(new Impl()) {}
Dungeon::~Dungeon() {
    stopSimulation();
    joinSimulation();
    delete pimpl_;
}

bool Dungeon::addNPC(std::unique_ptr<NPCBase> npc) {
    if (!npc) return false;
    if (npc->x() < 0 || npc->x() > 500 || npc->y() < 0 || npc->y() > 500) return false;

    std::lock_guard<std::shared_mutex> guard(pimpl_->npcs_mutex);
    auto it = std::find_if(pimpl_->npcs.begin(), pimpl_->npcs.end(),
                           [&](const std::shared_ptr<NPCBase> &p){ return p->name() == npc->name(); });
    if (it != pimpl_->npcs.end()) return false;

    std::shared_ptr<NPCBase> sp(std::move(npc));
    pimpl_->npcs.push_back(std::move(sp));
    return true;
}

bool Dungeon::loadFromFile(const std::string &fname) {
    std::ifstream f(fname);
    if (!f) return false;
    std::string line;
    std::vector<std::shared_ptr<NPCBase>> newlist;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto up = NPCFactory::createFromLine(line);
        if (!up) continue;
        if (up->x() < 0 || up->x() > 500 || up->y() < 0 || up->y() > 500) continue;
        bool dup = std::any_of(newlist.begin(), newlist.end(), [&](auto &p){ return p->name() == up->name(); });
        if (dup) continue;
        newlist.push_back(std::shared_ptr<NPCBase>(std::move(up)));
    }
    {
        std::lock_guard<std::shared_mutex> guard(pimpl_->npcs_mutex);
        pimpl_->npcs = std::move(newlist);
    }
    return true;
}

bool Dungeon::saveToFile(const std::string &fname) const {
    std::ofstream f(fname);
    if (!f) return false;
    std::shared_lock<std::shared_mutex> sguard(pimpl_->npcs_mutex);
    for (auto &p : pimpl_->npcs) {
        f << p->type() << " " << p->name() << " " << p->x() << " " << p->y() << "\n";
    }
    return true;
}

void Dungeon::clear() noexcept {
    std::lock_guard<std::shared_mutex> guard(pimpl_->npcs_mutex);
    pimpl_->npcs.clear();
}

void Dungeon::printAll() const {
    // Параметры карты — меняй под себя
    constexpr int GRID_W = 10;   // ширина в символических ячейках
    constexpr int GRID_H = 10;   // высота в ячейках
    constexpr double COORD_MAX = 100.0; // макс. координата по осям (твои NPC: 0..100)

    // подготовим пустую сетку (символ ' ' означает пустую клетку)
    std::vector<std::string> grid(GRID_H, std::string(GRID_W, ' '));
    int alive_count = 0;

    // Считываем NPC под shared_lock (несколько потоков могут читать)
    {
        std::shared_lock<std::shared_mutex> lock(pimpl_->npcs_mutex);
        for (const auto &p : pimpl_->npcs) {
            if (!p) continue;
            if (!p->alive()) continue;
            ++alive_count;

            // масштабирование координат 0..COORD_MAX -> 0..GRID_W-1 / GRID_H-1
            int gx = static_cast<int>(p->x() / COORD_MAX * GRID_W);
            int gy = static_cast<int>(p->y() / COORD_MAX * GRID_H);

            // защитимся от граничных случаев (на случай x==COORD_MAX)
            if (gx < 0) gx = 0;
            if (gy < 0) gy = 0;
            if (gx >= GRID_W) gx = GRID_W - 1;
            if (gy >= GRID_H) gy = GRID_H - 1;

            // выбираем символ по типу; при конфликте ставим '*'
            char symbol = '?';
            std::string t = p->type();
            if (t == "Bear" || t == "bear") symbol = 'B';
            else if (t == "Bandit" || t == "bandit") symbol = 'b';
            else if (t == "Orc" || t == "orc") symbol = 'O';
            else if (t == "Werewolf" || t == "werewolf") symbol = 'W';
            else if (t == "Squirrel" || t == "squirrel") symbol = 'S';
            else symbol = t.empty() ? '?' : t[0];

            char &cell = grid[gy][gx];
            if (cell == ' ') cell = symbol;
            else if (cell == symbol) { /* same symbol stays */ }
            else cell = '*'; // collision: multiple NPCs in same cell
        }
    }

    // Печатаем карту под lock'ом cout (защита вывода)
    {
        std::lock_guard<std::mutex> cout_lock(coutMutex()); // теперь coutMutex() - const
        std::cout << "--- NPCs (" << alive_count << ") ---\n";

        // печатаем сверху-вниз: первая строка y=0
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                std::cout << "[" << grid[y][x] << "]";
            }
            std::cout << '\n';
        }
        std::cout << std::flush;
    }
}

EventManager& Dungeon::events() noexcept {
    return pimpl_->events;
}

// legacy runCombat (keeps single-thread semantics)
void Dungeon::runCombat(double range) {
    if (range < 0.0) return;
    const double r2 = range * range;

    std::vector<std::shared_ptr<NPCBase>> npcs_snapshot;
    {
        std::shared_lock<std::shared_mutex> sguard(pimpl_->npcs_mutex);
        npcs_snapshot = pimpl_->npcs;
    }

    size_t n = npcs_snapshot.size();
    if (n < 2) return;

    std::vector<char> aliveAtStart(n, 0);
    for (size_t i = 0; i < n; ++i) aliveAtStart[i] = npcs_snapshot[i]->alive() ? 1 : 0;

    std::vector<char> willDie(n, 0);
    std::vector<std::string> killerOf(n);
    struct Ev { std::string killer; std::string victim; double x; double y; };
    std::vector<Ev> events;

    for (size_t i = 0; i < n; ++i) {
        if (!aliveAtStart[i]) continue;
        for (size_t j = i+1; j < n; ++j) {
            if (!aliveAtStart[j]) continue;
            double dx = npcs_snapshot[i]->x() - npcs_snapshot[j]->x();
            double dy = npcs_snapshot[i]->y() - npcs_snapshot[j]->y();
            if (dx*dx + dy*dy > r2) continue;

            CombatVisitor cv_i(npcs_snapshot[i].get());
            npcs_snapshot[j]->accept(cv_i);
            if (cv_i.victimDies()) {
                willDie[j] = 1;
                if (killerOf[j].empty()) {
                    killerOf[j] = npcs_snapshot[i]->name();
                    events.push_back({npcs_snapshot[i]->name(), npcs_snapshot[j]->name(), npcs_snapshot[j]->x(), npcs_snapshot[j]->y()});
                }
            }
            if (cv_i.attackerDies()) {
                willDie[i] = 1;
                if (killerOf[i].empty()) {
                    killerOf[i] = npcs_snapshot[j]->name();
                    events.push_back({npcs_snapshot[j]->name(), npcs_snapshot[i]->name(), npcs_snapshot[i]->x(), npcs_snapshot[i]->y()});
                }
            }
        }
    }

    // apply under exclusive lock
    {
        std::lock_guard<std::shared_mutex> guard(pimpl_->npcs_mutex);
        for (size_t idx = 0; idx < n; ++idx) {
            if (willDie[idx]) {
                // find by name and mark dead
                for (auto &p : pimpl_->npcs) {
                    if (p->name() == npcs_snapshot[idx]->name() && p->alive()) {
                        p->markDead();
                        break;
                    }
                }
            }
        }
        // remove dead
        pimpl_->npcs.erase(std::remove_if(pimpl_->npcs.begin(), pimpl_->npcs.end(),
                    [](const std::shared_ptr<NPCBase> &p){ return !p->alive(); }), pimpl_->npcs.end());
    }

    for (auto &ev : events) {
        pimpl_->events.notify({ev.killer, ev.victim, ev.x, ev.y});
    }
}

// Simulation helpers ----------------------------------------------------------------
namespace {
    // movement & kill distance tables (map values from assignment) - used by simulation threads
    const std::unordered_map<std::string,int> MOVE_DIST = {
        {"Orc", 20}, {"Squirrel", 5}, {"Bear", 5}, {"Bandit", 10}, {"Werewolf", 40}
    };
    const std::unordered_map<std::string,int> KILL_DIST = {
        {"Orc", 10}, {"Squirrel", 5}, {"Bear", 10}, {"Bandit", 10}, {"Werewolf", 5}
    };

    // wrapper for double-dispatch check: create temp objects via factory, use CombatVisitor
    bool wantsKill(const std::string &A, const std::string &B) {
        auto att = NPCFactory::create(A, std::string("__tmp_att__"), 0.0, 0.0);
        auto def = NPCFactory::create(B, std::string("__tmp_def__"), 0.0, 0.0);
        if (!att || !def) return false;
        CombatVisitor v(att.get());
        def->accept(v);
        return v.victimDies();
    }
}

// startSimulation launches movement and battle threads, runs for `seconds` or until stopSimulation called.
// If seconds<=0, threads run until stopSimulation() invoked.
void Dungeon::startSimulation(int seconds) {
    if (pimpl_->movement_thread.joinable() || pimpl_->battle_thread.joinable()) return;

    pimpl_->stop_flag.store(false);

    // movement thread
    pimpl_->movement_thread = std::thread([this]() {
        thread_local std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> ang(0.0, 2.0 * M_PI);
        const int tick_ms = 200;

        while (!pimpl_->stop_flag.load()) {
            // move
            {
                std::lock_guard<std::shared_mutex> lg(pimpl_->npcs_mutex);
                for (auto &p : pimpl_->npcs) {
                    if (!p || !p->alive()) continue;
                    int md = 5;
                    auto it = MOVE_DIST.find(p->type());
                    if (it != MOVE_DIST.end()) md = it->second;
                    double theta = ang(rng);
                    double nx = p->x() + md * std::cos(theta);
                    double ny = p->y() + md * std::sin(theta);
                    if (nx < 0.0) nx = 0.0;
                    if (nx > 100.0) nx = 100.0;
                    if (ny < 0.0) ny = 0.0;
                    if (ny > 100.0) ny = 100.0;
                    p->setPosition(nx, ny);
                }
            }

            // detect pairs and enqueue fights (read-only snapshot)
            {
                std::shared_lock<std::shared_mutex> sguard(pimpl_->npcs_mutex);
                size_t n = pimpl_->npcs.size();
                std::set<std::pair<std::string,std::string>> seen;
                for (size_t i = 0; i < n; ++i) {
                    auto A = pimpl_->npcs[i];
                    if (!A || !A->alive()) continue;
                    for (size_t j = i+1; j < n; ++j) {
                        auto B = pimpl_->npcs[j];
                        if (!B || !B->alive()) continue;
                        double dx = A->x() - B->x();
                        double dy = A->y() - B->y();
                        double dist2 = dx*dx + dy*dy;
                        double kdA = 5.0, kdB = 5.0;
                        auto ita = KILL_DIST.find(A->type());
                        if (ita != KILL_DIST.end()) kdA = ita->second;
                        auto itb = KILL_DIST.find(B->type());
                        if (itb != KILL_DIST.end()) kdB = itb->second;
                        double maxkd = std::max(kdA, kdB);
                        if (dist2 <= maxkd * maxkd) {
                            bool A_w = wantsKill(A->type(), B->type());
                            bool B_w = wantsKill(B->type(), A->type());
                            if (!A_w && !B_w) continue;
                            auto key = (A->name() < B->name()) ? std::make_pair(A->name(), B->name())
                                                               : std::make_pair(B->name(), A->name());
                            if (seen.find(key) != seen.end()) continue;
                            seen.insert(key);
                            {
                                std::lock_guard<std::mutex> ql(pimpl_->queue_mutex);
                                pimpl_->fight_queue.emplace_back(A, B);
                            }
                            pimpl_->queue_cv.notify_one();
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(tick_ms));
        }
    });

    // battle thread
    pimpl_->battle_thread = std::thread([this]() {
        thread_local std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count() + 12345);
        std::uniform_int_distribution<int> die(1,6);

        while (!pimpl_->stop_flag.load()) {
            std::pair<std::shared_ptr<NPCBase>, std::shared_ptr<NPCBase>> task;
            {
                std::unique_lock<std::mutex> ql(pimpl_->queue_mutex);
                if (pimpl_->fight_queue.empty()) {
                    pimpl_->queue_cv.wait_for(ql, std::chrono::milliseconds(250));
                }
                if (!pimpl_->fight_queue.empty()) {
                    task = pimpl_->fight_queue.front();
                    pimpl_->fight_queue.pop_front();
                } else {
                    if (pimpl_->stop_flag.load()) break;
                    continue;
                }
            }

            auto A = task.first;
            auto B = task.second;
            if (!A || !B) continue;

            // quick checks
            {
                std::shared_lock<std::shared_mutex> sguard(pimpl_->npcs_mutex);
                if (!A->alive() || !B->alive()) continue;
                double dx = A->x() - B->x();
                double dy = A->y() - B->y();
                double dist2 = dx*dx + dy*dy;
                double kdA = 5.0, kdB = 5.0;
                auto ita = KILL_DIST.find(A->type());
                if (ita != KILL_DIST.end()) kdA = ita->second;
                auto itb = KILL_DIST.find(B->type());
                if (itb != KILL_DIST.end()) kdB = itb->second;
                double maxkd = std::max(kdA, kdB);
                if (dist2 > maxkd * maxkd) continue;
            }

            bool A_dies = false;
            bool B_dies = false;

            // A attacks B if wantsKill
            if (wantsKill(A->type(), B->type())) {
                int atk = die(rng);
                int def = die(rng);
                if (atk > def) B_dies = true;
            }
            // B attacks A
            if (wantsKill(B->type(), A->type())) {
                int atk = die(rng);
                int def = die(rng);
                if (atk > def) A_dies = true;
            }

            {
                std::lock_guard<std::shared_mutex> lg(pimpl_->npcs_mutex);
                if (B_dies && B->alive()) {
                    B->markDead();
                    pimpl_->events.notify({A->name(), B->name(), B->x(), B->y()});
                }
                if (A_dies && A->alive()) {
                    A->markDead();
                    pimpl_->events.notify({B->name(), A->name(), A->x(), A->y()});
                }
                // remove dead
                pimpl_->npcs.erase(std::remove_if(pimpl_->npcs.begin(), pimpl_->npcs.end(),
                            [](const std::shared_ptr<NPCBase> &p){ return !p->alive(); }),
                           pimpl_->npcs.end());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // If seconds > 0, spawn a helper thread that will stop after seconds
    if (seconds > 0) {
        std::thread([this, seconds]() {
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            stopSimulation();
        }).detach();
    }
}

void Dungeon::stopSimulation() {
    pimpl_->stop_flag.store(true);
    pimpl_->queue_cv.notify_all();
}

void Dungeon::joinSimulation() {
    if (pimpl_->movement_thread.joinable()) pimpl_->movement_thread.join();
    if (pimpl_->battle_thread.joinable()) pimpl_->battle_thread.join();
}

std::mutex & Dungeon::coutMutex() const noexcept {
    return pimpl_->cout_mutex;
}