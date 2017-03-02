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
#include <string>
#include <list>
#include <stdexcept>
#include <iostream>

#include "Arg.h"

std::string mangle(const std::list<Arg>& fl)
{
    // for double sin(double) this returns "sin(d)"

    auto arg_it = fl.begin();  // The first argument is the function name/return type
	if (fl.end() == arg_it) throw std::runtime_error("mangle() called with empty argument list - function has no name.");

	std::string mangled_name=arg_it->arg_ID;
	mangled_name+="(";

    bool need_comma {false};
	for (arg_it++; arg_it != fl.end(); arg_it++)
	{
        if (need_comma) mangled_name+=",";
        need_comma = true;
		mangled_name += arg_it->arg_type.print();
	}
	mangled_name+=")";
	return mangled_name;
}

std::string printed_form(const std::list<Arg>& fl)
{
    // for double sin(double) this returns "double sin(double)"

    auto arg_it = fl.begin();  // The first argument is the function name/return type

	if (fl.end() == arg_it) throw std::runtime_error("mangle() called with empty argument list - function has no name.");

	std::string mangled_name = arg_it->arg_type.print() + std::string(" ") + arg_it -> arg_ID + std::string("(");

    bool need_comma {false};
	for (arg_it++; arg_it != fl.end(); arg_it++)
	{
        if (need_comma) mangled_name+=",";
        need_comma = true;
		mangled_name += arg_it->arg_type.print();
	}
	mangled_name+=")";
	return mangled_name;
}

std::ostream& operator<<(std::ostream& os, const std::list<Arg>& al)
{
	if (al.size() == 0)
	{
        os << "<null argument list>";
        return os;
	}

	const Arg& f = al.front();  // The first Arg has the function name and return value.
	os << f.arg_type << ' ' << f.arg_ID << '(';

    bool need_comma = false;
    auto it = al.begin();
    for (it++; it != al.end(); it++)
    {
    	if (need_comma) os << ", ";
    	need_comma = true;
		os << it->arg_type;
		if ((it->arg_ID.length())>0)
			os << ' ' << it->arg_ID;
	}
	return os << ')';
}

std::ostream& operator<<(std::ostream& os, const Arg& a)
{
    os << "Arg<type=" << a.arg_type.print() << ",id=\"" << a.arg_ID << "\">";
    return os;
}


const Type& return_type(const std::list<Arg>& fl) {
	if (fl.size() == 0) throw std::runtime_error("Arg.h: const Type& return_type(const std::list<Arg>& fl) called with empty argument list.");
	return fl.front().arg_type;
}
