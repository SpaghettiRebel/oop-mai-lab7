#pragma once
#include <string>
#include <vector>
#include <memory>


struct DeathEvent { 
    std::string killer; 
    std::string victim; 
    double x; 
    double y; 
};


class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onDeath(const DeathEvent &ev) = 0;
};


class EventManager {
public:
    void subscribe(std::shared_ptr<IObserver> observers_);
    void notify(const DeathEvent &ev) const;
private:
    std::vector<std::shared_ptr<IObserver>> observers_;
};
