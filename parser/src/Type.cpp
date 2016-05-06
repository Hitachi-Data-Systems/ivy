//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

bool Type::needs_constructor() const {
    // un-comment out later if we get more types
    // that need constructors / destructors
    return value_type == type_string;
    //switch (value_type) {
    //case type_string:
    //	return 1;
    //default:
    //	return 0;
    //};
}

bool Type::needs_destructor() const {
    return value_type == type_string;
    //switch (value_type) {
    //case type_string:
    //	return 1;
    //default:
    //	return 0;
    //}
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
