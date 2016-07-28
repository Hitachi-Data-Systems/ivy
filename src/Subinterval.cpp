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

#include <iostream>

#include "Subinterval.h"

std::ostream& operator<< (std::ostream& o, const subinterval_state s)
{
    switch (s)
    {
        case subinterval_state::undefined:
            o << "subinterval_state::undefined";
            break;
        case subinterval_state::ready_to_run:
            o << "subinterval_state::ready_to_run";
            break;
        case subinterval_state::running:
            o << "subinterval_state::running";
            break;
        case subinterval_state::ready_to_send:
            o << "subinterval_state::ready_to_send";
            break;
        case subinterval_state::sending:
            o << "subinterval_state::sending";
            break;
        case subinterval_state::stop:
            o << "subinterval_state::stop";
            break;
        default:
            o << "<internal programming error - unknown subinterval_state value>";
    }
    return o;
}

