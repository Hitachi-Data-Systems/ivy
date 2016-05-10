//Copyright (c) 2016 Hitachi Data Systems, Inc.
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
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
