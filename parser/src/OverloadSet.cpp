//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>

#include "OverloadSet.h"

OverloadTable builtin_overloads;

std::ostream& operator<<(std::ostream& os,const OverloadSet& overload_set)
{
    os << "{";
    bool first_pass{true};
    for (auto& o : overload_set)
    {
        if (!first_pass) os << ",";
        first_pass=false;
        os << " \"" << o.second << "\"";

    }
    if (!first_pass) os << " ";
    os << "}";

    return os;
}

std::ostream& operator<<(std::ostream& os,const OverloadTable& ot)
{
    os << "{";
    bool table_empty{true};
    for (auto& o : ot)
    {
        if (!table_empty) os << ','; table_empty=false;
        os << " \"" << o.first << "\" -> " << o.second;
    }
    os << "}";

    return os;
}

