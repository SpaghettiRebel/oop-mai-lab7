#include "observer.hpp"

void EventManager::subscribe(std::shared_ptr<IObserver> obs) {
    if (obs) observers_.push_back(std::move(obs));
}

void EventManager::notify(const DeathEvent &ev) const {
    for (auto &o : observers_) {
        if (o) o->onDeath(ev);
    }
}
