//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cassert>

#include "Type.h"

std::ostream& operator<< (std::ostream& os, const Type& t)
{
	return os << t.print();
}

std::string Type::print() const
{
    // "void", "int", "double", "string" or throw std::runtime_error()
    switch (value_type)
    {
        case type_void:   return "void";
        case type_int:    return "int";
        case type_double: return "double";
        case type_string: return "string";
        default:
        {
            std::ostringstream o;
            o << "Type::print() - unknown value_type decimal int value " << (int) value_type;
            throw std::runtime_error(o.str());
        }
    }
}

size_t Type::maatvan() const {
    switch (value_type) {
        case type_int: return sizeof(int);
        case type_double: return sizeof(double);
        case type_string: return sizeof(std::string);
        case type_void: return 0;
        default: throw std::runtime_error("Type::maatvan() - unknown type");
    }
    return 0;
}

bool Type::needs_constructor() const
{
    switch (value_type)
    {
        case type_int:    return true;
        case type_double: return true; // had a bad experience with a double that wasn't constructed
        case type_string: return true;
        case type_void:
            {
                std::ostringstream o;
                o << "<Error> in method Type::needs_constructor() at line " << __LINE__ << " of source file " << __FILE__
                    << " - \"needs_constructor()\" may not be called for a \"void\" type.";
                throw std::runtime_error(o.str());
            }
        default:
            {
                std::ostringstream o;
                o << "<Error> in method Type::needs_constructor() at line " << __LINE__ << " of source file " << __FILE__
                    << " - \"needs_constructor()\" was called for an unknown type.";
                throw std::runtime_error(o.str());
            }
    }
}

bool Type::needs_destructor() const {
    switch (value_type)
    {
        case type_int:    return false;
        case type_double: return false;
        case type_string: return true;
        case type_void:
            {
                std::ostringstream o;
                o << "<Error> in method Type::needs_desstructor() at line " << __LINE__ << " of source file " << __FILE__
                    << " - \"needs_constructor()\" may not be called for a \"void\" type.";
                throw std::runtime_error(o.str());
            }
        default:
            {
                std::ostringstream o;
                o << "<Error> in method Type::needs_desstructor() at line " << __LINE__ << " of source file " << __FILE__
                    << " - \"needs_constructor()\" was called for an unknown type.";
                throw std::runtime_error(o.str());
            }
    }
}

void Type::construct_at(void* p) const {
    switch (value_type)
    {
        case type_void   : return;
        case type_int    : *((int*)p)    = 0;   return;
        case type_double : *((double*)p) = 0.0; return;
        case type_string : new ((std::string*)p) std::string(); return;
        default:
            {
                std::ostringstream o;
                o << "Type::construct_at() - unknown value_type " << (int) value_type;
                throw std::runtime_error(o.str());
            }
    }
}

void Type::destroy_at(void* p) const
{
    if (value_type==type_string)
    {
        typedef std::string std_string;
        ((std::string*) p) -> ~std_string();
    }
    return;
}

int Type::both_numeric_or_both_string(const Type& t) const
{
    switch (value_type) {
    case type_int:
        switch (t.value_type) {
        case type_int:	return 1;
        case type_double:	return 1;
        case type_string:	return 0;
        default:
            {
                std::ostringstream o;
                o << "Type::both_numeric_or_both_string() - unknown value_type " << (int) t.value_type;
                throw std::runtime_error(o.str());
            }
        }
    case type_double:
        switch (t.value_type) {
        case type_int:	return 1;
        case type_double:	return 1;
        case type_string:	return 0;
        default:
            {
                std::ostringstream o;
                o << "Type::both_numeric_or_both_string() - unknown value_type " << (int) t.value_type;
                throw std::runtime_error(o.str());
            }
        }
    case type_string:
        switch (t.value_type) {
        case type_int:	return 0;
        case type_double:	return 0;
        case type_string:	return 1;
        default:
            {
                std::ostringstream o;
                o << "Type::both_numeric_or_both_string() - unknown value_type " << (int) t.value_type;
                throw std::runtime_error(o.str());
            }
        }
    default:
        {
            std::ostringstream o;
            o << "Type::both_numeric_or_both_string() - unknown value_type " << (int) value_type;
            throw std::runtime_error(o.str());
        }
    }
    return 0; // keep compiler happy
}


std::string mangle_name1arg(const std::string& name, const Type& t1) {
    // d.g. "sin(d)"
    if (name.size() == 0) throw std::runtime_error("mangle_name1arg() called with zero-length name.");
	std::string mangled_name(name);
	mangled_name+="(";
	mangled_name+=t1.print();
	mangled_name+=")";
	return mangled_name;
}

std::string mangle_name2args(const std::string& name, const Type& t1, const Type& t2) {
    if (name.size() == 0) throw std::runtime_error("mangle_name2args() called with zero-length name.");
	std::string mangled_name(name);
	mangled_name+="(";
	mangled_name+=t1.print();
	mangled_name+=",";
	mangled_name+=t2.print();
	mangled_name+=")";
	return mangled_name;
}

std::string mangle_name3args(const std::string& name, const Type& t1, const Type& t2, const Type& t3) {
    if (name.size() == 0) throw std::runtime_error("mangle_name2args() called with zero-length name.");
	std::string mangled_name(name);
	mangled_name+="(";
	mangled_name+=t1.print();
	mangled_name+=",";
	mangled_name+=t2.print();
	mangled_name+=",";
	mangled_name+=t3.print();
	mangled_name+=")";
	return mangled_name;
}
