//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <list>

#include "Xpr.h"
#include "location.hh"

class host_spec
{
public:
    yy::location bookmark;
    virtual void add_to_hostname_list(std::list<std::string>*) = 0;
    virtual void display(const std::string&, std::ostream&) = 0;
    virtual ~host_spec(){};
};


void get_host_list(std::list<host_spec*>*,std::list<std::string>*);

void display_host_spec_pointer_list(std::list<host_spec*>*, std::ostream&);


class individual_hostname : public host_spec
{
    // "sun159"  (more generally a string expression that evaluates to "sun159".)

public:
    std::string hostname {};  // this is where we put the result of evaluating the Xpr.
    Xpr* p_string_Xpr;

    individual_hostname(const yy::location& l, Xpr* p) : p_string_Xpr(p){bookmark=l;};

    virtual ~individual_hostname() { delete p_string_Xpr;};
    virtual void add_to_hostname_list(std::list<std::string>*) override;
    virtual void display(const std::string& indent, std::ostream& o) override
    {
        o << indent << "host_spec for individual hostname - string expression:";
        p_string_Xpr->display(indent+indent_increment, o);
    };
};

class individual_dotted_quad : public host_spec
{
    // 192.168.0.0

public:
    std::string dotted_quad {};  // this is where we put the result of evaluating the Xpr.

    individual_dotted_quad(const yy::location& l, const std::string& dq) : dotted_quad(dq) {bookmark = l;};

    virtual void add_to_hostname_list(std::list<std::string>* pL) override {pL->push_back(dotted_quad);};
    virtual void display(const std::string& indent, std::ostream& o) override {o << indent << "host_spec for dotted quad - \"" << dotted_quad << "\""; };
    virtual ~individual_dotted_quad() override {};
};

class hostname_range : public host_spec
{
    // "host0" to "host15"

public:
    std::string first_host {};  // this is where we put the result of evaluating the Xpr.
    Xpr* p_first_Xpr;

    std::string last_host {};  // this is where we put the result of evaluating the Xpr.
    Xpr* p_last_Xpr;

    hostname_range(const yy::location& l, Xpr* pX1, Xpr* pX2) : p_first_Xpr(pX1), p_last_Xpr(pX2){bookmark=l;};

    virtual void add_to_hostname_list(std::list<std::string>*) override;
    virtual void display(const std::string& indent, std::ostream& o) override
    {
        o << indent << "host_spec for hostname range - from ";
        p_first_Xpr->display(indent+indent_increment,o);
        o << " to ";
        p_last_Xpr->display(indent+indent_increment,o);
    };
    virtual ~hostname_range() override { delete p_first_Xpr; delete p_last_Xpr;};

};

