#pragma once

#include <string>
#include <vector>

struct loop_level
{
    unsigned int             current_index {0}; // if >= values.size(), we are done this pass through values.
    std::string              attribute     {};
    std::vector<std::string> values        {};
};

struct nestedit
{
    unsigned int            current_level;
    std::vector<loop_level> loop_levels;

//methods
    void reset();
    void clear();
    bool run_iteration();    // returns false when we have already run last iteration.
};
