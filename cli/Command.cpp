/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include <iostream>
#include <map>

Command::~Command()
{
}

typedef std::map<std::string, Command *> CommandMap;

// Implementing this by value would be dangerous,
// since we can't ensure its constructor runs before ours.
// We create this on the heap when we need it, which ensures it exists.
std::unique_ptr<CommandMap> gMap;

CommandRegistry::CommandRegistry(const char *name, Command *c)
{
    if (!gMap)
        gMap.reset(new CommandMap);

    if (gMap->end() != gMap->find(name))
        std::cerr << "warning: Duplicate command " << name << std::endl;

    (*gMap)[name] = c;
}

Command *
CommandRegistry::find(const std::string &name)
{
    if (!gMap)
        return nullptr;

    auto i = gMap->find(name);
    if (gMap->end() == i)
        return nullptr;

    return i->second;
}

void
CommandRegistry::print()
{
    if (!gMap)
        return;

    for (auto &i: *gMap)
        std::cout << i.second->name() << std::endl;
}
