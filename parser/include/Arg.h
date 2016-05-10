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
#pragma once

#include <stdexcept>
#include <list>

#include "Type.h"

class Arg { // function argument
	friend  std::ostream& operator<<(std::ostream&, const Arg& );
	friend  std::ostream& operator<<(std::ostream&, const std::list<Arg>& );
public:
	Type arg_type;
	std::string arg_ID;

	Arg(Type t=Type(type_void)): arg_type(t), arg_ID() {};
	Arg(Type t, const std::string& c): arg_type(t), arg_ID(c) {};
	Arg(const Arg& f): arg_type(f.arg_type), arg_ID(f.arg_ID) {};
};

// arglist first entry is function return type and name.

// Ivy_pgm::declare_function() takes the bare list of just the arguments
// and inserts at the beginning the (return type, function name) node.

// Only in such function declarations the bare argument names (arg_ID) are possibly "".

const Type& return_type(const std::list<Arg>& fl);

typedef std::list<Arg> arglist;

std::string mangle(const std::list<Arg>& fl);
std::string printed_form(const std::list<Arg>& fl);
