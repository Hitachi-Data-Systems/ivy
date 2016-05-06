//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <list>
#include <iostream>

#include "Select.h"
#include "SelectClause.h"
#include "LUN.h"
#include "ivyhelpers.h"

void Select::display(const std::string& indent, std::ostream& o)
{
    if (p_SelectClause_pointer_list == nullptr)
    {
        o << indent << "Select has null pointer to SelectClause pointer list." << std::endl;
    }
    else
    {
        o << indent << "Select has " << p_SelectClause_pointer_list->size() << " entries:" << std::endl;

        for (auto& p : (*p_SelectClause_pointer_list))
        {
            p->display(indent+indent_increment,o);
        }
    }
}

bool Select::matches(LUN* pLUN)
{
    if (p_SelectClause_pointer_list == nullptr) return true; // null Select always matches

    for (auto& p : (*p_SelectClause_pointer_list))
    {
        if (!p->matches(pLUN)) return false;
    }

    return true;
}

bool Select::contains_attribute(const std::string& a)
{
    if (p_SelectClause_pointer_list != nullptr)
    {
        for (auto& pSelectClause : *p_SelectClause_pointer_list)
        {
            if (stringCaseInsensitiveEquality(a,pSelectClause->attribute_name_token()))
            {
                return true;
            }
        }
    }
    return false;
}
