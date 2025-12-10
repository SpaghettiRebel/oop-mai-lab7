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
    std::vector<std::shared_ptr<NPCBase>> npcs;
    EventManager events;

    mutable std::shared_mutex npcs_mutex;
    std::mutex cout_mutex;
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
    constexpr int GRID_W = 10;
    constexpr int GRID_H = 10;
    constexpr double COORD_MAX = 100.0;

    std::vector<std::string> grid(GRID_H, std::string(GRID_W, ' '));
    int alive_count = 0;

    {
        std::shared_lock<std::shared_mutex> lock(pimpl_->npcs_mutex);
        for (const auto &p : pimpl_->npcs) {
            if (!p) continue;
            if (!p->alive()) continue;
            ++alive_count;

            int gx = static_cast<int>(p->x() / COORD_MAX * GRID_W);
            int gy = static_cast<int>(p->y() / COORD_MAX * GRID_H);

            if (gx < 0) gx = 0;
            if (gy < 0) gy = 0;
            if (gx >= GRID_W) gx = GRID_W - 1;
            if (gy >= GRID_H) gy = GRID_H - 1;

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
            else if (cell == symbol) {}
            else cell = '*';
        }
    }

    {
        std::lock_guard<std::mutex> cout_lock(coutMutex());
        std::cout << "--- NPCs (" << alive_count << ") ---\n";

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

static bool checkKillByType(const std::string &A, const std::string &B) {
    if (A == "Orc") return (B == "Bear" || B == "Orc" || B == "Bandit");
    if (A == "Bear") return (B == "Squirrel");
    if (A == "Squirrel") return false;
    if (A == "Bandit") return (B == "Werewolf");
    if (A == "Werewolf") return (B == "Bandit");
    return false;
}

void Dungeon::startSimulation(int seconds) {
    if (pimpl_->movement_thread.joinable() || pimpl_->battle_thread.joinable()) return;

    pimpl_->stop_flag.store(false);

    // поток перемещений
    pimpl_->movement_thread = std::thread([this]() {
        thread_local std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<double> ang(0.0, 2.0 * M_PI);
        const int tick_ms = 200;

        while (!pimpl_->stop_flag.load()) {
            {
                std::lock_guard<std::shared_mutex> lg(pimpl_->npcs_mutex);
                for (auto &p : pimpl_->npcs) {
                    if (!p || !p->alive()) continue;
                    
                    int md = p->moveDistance();
                    
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

            {
                std::shared_lock<std::shared_mutex> sguard(pimpl_->npcs_mutex);
                size_t n = pimpl_->npcs.size();
                std::set<std::pair<std::string,std::string>> seen_in_tick;

                for (size_t i = 0; i < n; ++i) {
                    auto A = pimpl_->npcs[i];
                    if (!A || !A->alive()) continue;

                    for (size_t j = i+1; j < n; ++j) {
                        auto B = pimpl_->npcs[j];
                        if (!B || !B->alive()) continue;

                        double dx = A->x() - B->x();
                        double dy = A->y() - B->y();
                        double dist2 = dx*dx + dy*dy;
                        
                        double kdA = A->killDistance();
                        double kdB = B->killDistance();
                        double maxkd = std::max(kdA, kdB);

                        if (dist2 <= maxkd * maxkd) {
                            bool A_kills_B = checkKillByType(A->type(), B->type());
                            bool B_kills_A = checkKillByType(B->type(), A->type());

                            if (!A_kills_B && !B_kills_A) continue;

                            auto key = (A->name() < B->name()) ? std::make_pair(A->name(), B->name())
                                                               : std::make_pair(B->name(), A->name());
                            
                            if (seen_in_tick.find(key) == seen_in_tick.end()) {
                                seen_in_tick.insert(key);
                                {
                                    std::lock_guard<std::mutex> ql(pimpl_->queue_mutex);
                                    pimpl_->fight_queue.emplace_back(A, B);
                                }
                                pimpl_->queue_cv.notify_one();
                            }
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(tick_ms));
        }
    });

    // поток боя
    pimpl_->battle_thread = std::thread([this]() {
        thread_local std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count() + 12345);
        std::uniform_int_distribution<int> die(1,6);

        while (!pimpl_->stop_flag.load()) {
            std::pair<std::shared_ptr<NPCBase>, std::shared_ptr<NPCBase>> task;
            
            {
                std::unique_lock<std::mutex> ql(pimpl_->queue_mutex);
                pimpl_->queue_cv.wait(ql, [this](){ 
                    return !pimpl_->fight_queue.empty() || pimpl_->stop_flag.load(); 
                });
                
                if (pimpl_->stop_flag.load() && pimpl_->fight_queue.empty()) break;
                
                task = pimpl_->fight_queue.front();
                pimpl_->fight_queue.pop_front();
            }

            auto A = task.first;
            auto B = task.second;

            std::lock_guard<std::shared_mutex> lg(pimpl_->npcs_mutex);
            
            if (!A->alive() || !B->alive()) continue;
            
            double dx = A->x() - B->x();
            double dy = A->y() - B->y();
            double dist2 = dx*dx + dy*dy;
            double maxRange = std::max(A->killDistance(), B->killDistance());
            if (dist2 > maxRange * maxRange) continue;

            bool A_wins = false;
            bool B_wins = false;

            if (checkKillByType(A->type(), B->type())) {
                if (die(rng) > die(rng)) A_wins = true;
            }
            if (checkKillByType(B->type(), A->type())) {
                if (die(rng) > die(rng)) B_wins = true;
            }

            if (A_wins && !B_wins) {
                B->markDead();
                pimpl_->events.notify({A->name(), B->name(), B->x(), B->y()});
            } else if (B_wins && !A_wins) {
                A->markDead();
                pimpl_->events.notify({B->name(), A->name(), A->x(), A->y()});
            } else if (A_wins && B_wins) {
                A->markDead();
                B->markDead();
                pimpl_->events.notify({A->name(), B->name(), B->x(), B->y()});
                pimpl_->events.notify({B->name(), A->name(), A->x(), A->y()});
            }
        }
    });

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