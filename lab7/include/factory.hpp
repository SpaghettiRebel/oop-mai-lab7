#pragma once
#include <memory>
#include <string>


class NPCBase;


class NPCFactory {
public:
    static std::unique_ptr<NPCBase> create(const std::string &type, const std::string &name, double x, double y);
    static std::unique_ptr<NPCBase> createFromLine(const std::string &line);
};
