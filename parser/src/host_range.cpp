//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>
#include <string>
#include <regex>

#include "Ivy_pgm.h"

#include "host_range.h"
#include "ivyhelpers.h"

void get_host_list(std::list<host_spec*>* phsl,std::list<std::string>* phl)
{
    if (phsl == nullptr) {throw std::runtime_error("host_range.cpp - get_host_list() called with null pointer to list of host_spec pointers."   );}
    if (phl  == nullptr) {throw std::runtime_error("host_range.cpp - get_host_list() called with null pointer to list of std::string hostnames.");}

    for (auto& p : *phsl) p->add_to_hostname_list(phl);

    return;
}

void display_host_spec_pointer_list(std::list<host_spec*>* p, std::ostream& o)
{
    if (p == nullptr)
    {
        o << "null pointer to host_spec list." << std::endl;
    }
    else if (p->size() == 0)
    {
        o << "empty host_spec pointer list";
    }
    else
    {
        o << "host_spec pointer list( ";
        bool need_comma{false};
        for (auto& q : *p)
        {
            if (need_comma) o << ',';
            o << " ";
            q->display("",o);
            o << " )";
        }
    }
}

void describe_host_specification(std::ostream& o)
{
    o   << std::endl
        << "In the [Hosts] statement, valid hostnames look like an identifier, meaning they start with a letter," << std::endl
        << "and the remaining characters are letters, digits, and underscore _ characters." << std::endl
        << std::endl
        << "Dotted quad IP addresses like 192.168.0.0 are OK instead of a hostname." << std::endl
        << std::endl
        << "Ivy uses the name you specify, rather than the canonical hostname.  This is so that you" << std::endl
        << "can test ivy as if it were using multiple hosts by separately specifying multiple aliases / IP addresses" << std::endl
        << "for the same host." << std::endl
        << std::endl
        << "There is a shorthand for writing the names of a series of hosts where the names only differ in a consecutive" << std::endl
        << "numeric suffix.  Instead of saying"
        << R"(   [Hosts] "cb4" "cb5" "cb6" "cb7" [select] "serial_number" = "123456";)" << std::endl
        << "you can say" << std::endl
        << R"(   [Hosts] "cb4" to "cb7" [select] "serial_number" = "123456";)" << std::endl
        << "or you can say" << std::endl
        << R"(   [Hosts] "cb4" to "7" [select] "serial_number" = "123456";)" << std::endl
        << "or you can even say" << std::endl
        << R"(   [Hosts] "cb4" to 7 [select] "serial_number" = "123456";)" << std::endl
        << "as where strings are expected, if you specify a numeric value it will be converted to a string." << std::endl
    ;
}

void individual_hostname::add_to_hostname_list(std::list<std::string>* pOut)
{
    if (pOut == nullptr) p_Ivy_pgm->error("internal programming error: individual_hostname::add_to_hostname_list called with null pointer.\n");

    p_string_Xpr->evaluate((void*) (&hostname));
    if ( (!std::regex_match(hostname,identifier_regex)) && (!std::regex_match(hostname,dotted_quad_regex)) )
    {
        std::ostringstream o;

        o << "Invalid hostname \"" << hostname << "\" at " << bookmark << "." << std::endl;

        describe_host_specification(o);

        p_Ivy_pgm->error(o.str());
    }
    else
    {
        pOut->push_back(hostname);

        if (trace_evaluate)
        {
            trace_ostream << "individual_hostname::add_to_hostname_list() at " << bookmark << " pushed back \"" << hostname << "\"." << std::endl;
            trace_ostream << "List now contains {";
            for (auto& s : *pOut) { trace_ostream << "  \"" << s << "\""; }
            trace_ostream << " }" << std::endl;
        }
    }
}

void hostname_range::add_to_hostname_list(std::list<std::string>* pOut)
{
    p_first_Xpr->evaluate((void*) (&first_host));
    p_last_Xpr ->evaluate((void*) (&last_host ));

    if (trace_evaluate) { trace_ostream << "hostname_range::add_to_hostname_list() from \"" << first_host << "\" to \"" << last_host << "\"." << std::endl; }

    if (!std::regex_match(first_host,identifier_trailing_digits_regex))
    {
        std::ostringstream o;

        o   << "[Hosts] statement error - invalid range of hostnames with a consecutive numeric suffix at " << bookmark << "." << std::endl
            << "In the specified hostname range, which evaluated to:  \"" << first_host << "\" to \"" << last_host << "\":" << std::endl
            << "Invalid first hostname \"" << first_host << "\"." << std::endl;

        describe_host_specification(o);

        p_Ivy_pgm->error(o.str());

        return;
    }

    std::regex id_numeric_suffix_regex("(.*[^\\d])(\\d+)");
    std::smatch smash;

    // we already know that it matches a hostname with trailing digits.

    if (!std::regex_match(first_host,smash,id_numeric_suffix_regex))
    {
        p_Ivy_pgm->error("hostname_range::add_to_hostname_list() - internal programmming error. This cannot happen.\n");
        return;
    }

    std::string prefix;
    int first,last;

    std::ssub_match prefix_match = smash[1];
    prefix = prefix_match.str();

    std::ssub_match suffix_match = smash[2];
    std::string suffix = suffix_match.str();

    first = atoi(suffix.c_str());

    if (trace_evaluate) { trace_ostream << "hostname_range::add_to_hostname_list() at " << bookmark << " first host prefix \"" << prefix << "\" suffix (int) " << first << "." << std::endl; }

    if (std::regex_match(last_host,digits_regex))
    {
        last = atoi(last_host.c_str());

        if (trace_evaluate) { trace_ostream << "hostname_range::add_to_hostname_list()  at " << bookmark << " last host was digits only which turned into (int) " << last << "." << std::endl; }

    }
    else if (!regex_match(last_host,identifier_trailing_digits_regex))
    {
        std::ostringstream o;

        o   << "[Hosts] statement error at " << bookmark << " - invalid range of hostnames with a consecutive numeric suffix." << std::endl
            << "In the specified hostname range, which evaluated to:  \"" << first_host << "\" to \"" << last_host << "\"" << std::endl
            << "Invalid last hostname or number part of last hostname \"" << last_host << "\"." << std::endl;

        describe_host_specification(o);

        p_Ivy_pgm->error(o.str());

        return;
    }
    else
    {
        // last hostname is hostname with trailing digits - make sure prefix part is the same.

        std::string prefix2;

        if (!std::regex_match(last_host,smash,id_numeric_suffix_regex))
        {
            p_Ivy_pgm->error("hostname_range::add_to_hostname_list() - internal programmming error. This cannot happen.\n");
        }
        prefix_match = smash[1];
        prefix2 = prefix_match.str();

        suffix_match = smash[2];
        suffix = suffix_match.str();

        if (trace_evaluate) { trace_ostream << "hostname_range::add_to_hostname_list() at " << bookmark << " last host prefix \"" << prefix2 << "\" suffix (string) " << suffix << "." << std::endl; }

        if (prefix != prefix2)
        {
            std::ostringstream o;

            o   << "[Hosts] statement error at " << bookmark << " - invalid range of hostnames with a consecutive numeric suffix." << std::endl
                << "In the specified hostname range, which evaluated to:  \"" << first_host << "\" to \"" << last_host << "\"" << std::endl
                << "If the full last hostname is given, the body of the hostname before the numeric suffix must be the same in the first and last hostnames." << std::endl;

            describe_host_specification(o);

            p_Ivy_pgm->error(o.str());

            return;
        }

        last = atoi(suffix.c_str());

    }

    if (first >= last)
    {
        std::ostringstream o;
        o << "[Hosts] statement error at " << bookmark << " - invalid range of hostnames with a numeric suffix.  In \"" << first_host << " to " << last_host << "\", the ending number must be higher than the starting number." << std::endl;
        p_Ivy_pgm ->error(o.str());
        return;
    }

    for (int i = first; i <= last; i++)
    {
        std::ostringstream o;
        o << prefix << i;
        pOut->push_back(o.str());

        if (trace_evaluate)
        {
            trace_ostream << "hostname_range::add_to_hostname_list() at " << bookmark << " pushed back \"" << o.str() << "\". List now contains";
            for (auto& s : *pOut) { trace_ostream << "  \"" << s << "\""; }
        }
    }

    return;
}
