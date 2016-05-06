//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>
#include <list>

#include "LUN.h"
#include "SelectClause.h"
#include "LDEVset.h"
#include "Matcher.h"
#include "ivyhelpers.h"
#include "master_stuff.h"

std::string SelectClause::attribute_name_token()
{
    std::string s;
    p_attribute_name_Xpr->evaluate((void*)&s);
    return column_header_to_identifier(s);
}

std::list<std::string> SelectClause::attribute_value_tokens()
{
    std::list<std::string> l {};

    if (p_SelectClauseValue_pointer_list == nullptr) return l;

    for (auto& p_scv : (*p_SelectClauseValue_pointer_list))
    {
        Xpr* p_x = p_scv->pXpr;
        if (p_x == nullptr) continue;
        else
        {
            std::string s;
            p_x->evaluate((void*)&s);
            l.push_back(s);
        }
    }
    return l;
}

void SelectClauseValue::display(const std::string& indent, std::ostream& o)
{
    o << indent << "SelectClauseValue at " << bookmark << " value expression is:" << std::endl;

    if (pXpr != nullptr)
    {
        pXpr->display(indent+indent_increment,o);
    }
    else
    {
        o << indent << indent_increment << "<< SelectClauseValue\'s pointer to Xpr is null pointer. >>" << std::endl;
    }
}

void display_SelectClauseValue_pointer_list(const std::string& indent, std::list<SelectClauseValue*>* p, std::ostream& o)
{
    o << indent << "std::list<SelectClauseValue*>*";

    if (p == nullptr)
    {
        o << " is nullptr." << std::endl;
        return;
    }

    o << " -> size() = " << p->size() << std::endl;

    for (auto& pv : *p)
    {
        pv->display(indent+indent_increment,o);
    }
}


bool SelectClause::matches(LUN* p_LUN)
{
	std::string attribute_name = attribute_name_token();

    if (stringCaseInsensitiveEquality("LDEV",attribute_name)) return ldev_matches(p_LUN);
    if (stringCaseInsensitiveEquality("host",attribute_name)) return host_matches(p_LUN);
    if (stringCaseInsensitiveEquality("PG",  attribute_name)) return pg_matches  (p_LUN);
    return default_matches(p_LUN);
}


bool SelectClause::default_matches(LUN* pLUN)
{
    std::string attribute_name, lun_attribute_value, select_clause_value;

    if ( pLUN == nullptr)
    {
        std::ostringstream o;
        o << "SelectClause::default_matches() - pLUN == nullptr" << std::endl;
        std::cout << o.str();
        fileappend(logfile, o.str());
        throw std::runtime_error(o.str());
            // I took out call to p_Ivy_pgm->error() because this gets used in ivyslave, and it's developer diagnostic message anyway ...
    }
    if ( p_attribute_name_Xpr == nullptr)
    {
        std::ostringstream o;
        o << "SelectClause::default_matches() - p_attribute_name_Xpr == nullptr" << std::endl;
        std::cout << o.str();
        fileappend(logfile, o.str());
        throw std::runtime_error(o.str());
            // I took out call to p_Ivy_pgm->error() because this gets used in ivyslave, and it's developer diagnostic message anyway ...
    }
    if ( p_SelectClauseValue_pointer_list == nullptr)
    {
        std::ostringstream o;
        o << "SelectClause::default_matches() - p_SelectClauseValue_pointer_list == nullptr" << std::endl;
        std::cout << o.str();
        fileappend(logfile, o.str());
        throw std::runtime_error(o.str());
            // I took out call to p_Ivy_pgm->error() because this gets used in ivyslave, and it's developer diagnostic message anyway ...
    }

    p_attribute_name_Xpr->evaluate((void*)&attribute_name);
    attribute_name = column_header_to_identifier(attribute_name);

    lun_attribute_value = pLUN->attribute_value(attribute_name);

    for (auto& pSelectClauseValue : *p_SelectClauseValue_pointer_list)
    {
        if (pSelectClauseValue == nullptr)
        {
            std::ostringstream o;
            o << "SelectClause::default_matches() - a nullptr was contained in the list of SelectClauseValue pointers." << std::endl;
            std::cout << o.str();
            fileappend(logfile, o.str());
            throw std::runtime_error(o.str());
                // I took out call to p_Ivy_pgm->error() because this gets used in ivyslave, and it's developer diagnostic message anyway ...
        }
        if (stringCaseInsensitiveEquality(pSelectClauseValue->value(),lun_attribute_value)) return true;
    }

    return false;
}


bool SelectClause::ldev_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
		if (trace_evaluate) std::cout << "SelectClause::ldev_matches called with NULL pointer to LUN." << std::endl;
		return false;
	}

	std::string lun_ldev_string = p_LUN->attribute_value("LDEV");

	trim(lun_ldev_string); // remove leading/trailing whitespace
	if (lun_ldev_string.size() == 0)
	{
        if (trace_evaluate) { trace_ostream << "SelectClause::ldev_matches() - LUN did not have the LDEV attribute or its value was at most whitespace." << std::endl; }
        return false;
	}

    int lun_ldev_int = LDEVset::LDEVtoInt(lun_ldev_string);

	if (-1 == lun_ldev_int)
	{
        if (trace_evaluate) { trace_ostream << "SelectClause::ldev_matches() - LUN LDEV attribute value \"" << lun_ldev_string << "\" did not parse as an LDEV." << std::endl; }
        return false;
	}

    LDEVset ls {};

    for (auto ldev_set_token: attribute_value_tokens())
    {
        trim(ldev_set_token);

        if (ldev_set_token.size() == 0) continue;

        if (!ls.add(ldev_set_token,logfile))
        {
            std::cout << "[Select] clause LDEV shorthand match failure - LDEV match specification \"" << ldev_set_token << "\" did not parse as an LDEVset." << std::endl;

            if (trace_evaluate) { trace_ostream << "[Select] clause LDEV shorthand match failure - LDEV match specification \""
                << ldev_set_token << "\" did not parse as an LDEVset." << std::endl;}

            return false;
        }

    }

    return (ls.seen.end() != ls.seen.find(lun_ldev_int));
}


std::string digits {"([[:digit:]]+)"};
std::regex regex_digits (digits);

std::string asterisk {"(\\*)"};
std::regex regex_asterisk (asterisk);

std::string colon {"(:)"};
std::regex regex_colon(colon);

std::string to_range {digits+colon};
std::regex regex_to_range (to_range);

std::string from_range {colon+digits};
std::regex regex_from_range (from_range);

std::string range {digits+colon+digits};
std::regex regex_range (range);

std::string each_side
{
    std::string("(")
        + digits + std::string("|")
        + asterisk + std::string("|")
        + range + std::string("|")
        + to_range + std::string("|")
        + from_range
    + std::string(")")
};

std::regex regex_PG (digits + std::string("-") + digits);
std::regex regex_before_after_hyphen ("([^-]*)-([^-]*)");
std::regex regex_PG_match ( asterisk + std::string("|(") + each_side + std::string("-") + each_side + std::string(")") );


bool pg_part_matches(int i,const std::string& part)
{
    std::smatch smash;

    if (std::regex_match(part,regex_asterisk))
    {
        return true;
    }
    else if (std::regex_match(part,smash,regex_digits))
    {
        return (i == atoi(part.c_str()));
    }
    else if (std::regex_match(part,smash,regex_range))
    {
        std::ssub_match start_ssub = smash[1];
        std::ssub_match end_ssub = smash[3];
        int start_int = atoi(start_ssub.str().c_str());
        int end_int   = atoi(  end_ssub.str().c_str());

        if ( i >= start_int && i <= end_int)
            return true;
        else
            return false;
    }
    else if (std::regex_match(part,smash,regex_from_range))
    {
        std::ssub_match start_ssub = smash[2];
        int start_int = atoi(start_ssub.str().c_str());

        if ( i >= start_int)
            return true;
        else
            return false;
    }
    else if (std::regex_match(part,smash,regex_to_range))
    {
        std::ssub_match end_ssub = smash[1];
        int end_int = atoi(end_ssub.str().c_str());

        if ( i <= end_int)
            return true;
        else
            return false;
    }
    else
    {
        std::string m {"SelectClause.cpp - pg_part_matches() - this is not supposed to be able to happen.\n"};
        std::cout << m;
        log("~/ivy_developer_diagnostic_log.txt", m);
        throw std::runtime_error(m);
            // I took out call to p_Ivy_pgm->error() because this gets used in ivyslave, and it's developer diagnostic message anyway ...
    }
    return false; // unreachable - put this here to stop compiler messate
}


void SelectClause::complain_pg(std::string s)
{
        std::ostringstream o;
        o << "SelectClause at " << bookmark << " - PG match specification \"" << s << "\" is invalid." << std::endl
            << "Examples of valid PG match specifications:" << std::endl
            << "   *" << std::endl
            << "        - matches on all PGs." << std::endl
            << "   1-*" << std::endl
            << "        - matches on any parity group starting with \"1-\"." << std::endl
            << "   1-2:3" << std::endl
            << "        - matches on \"1-2\" and \"1-3\".." << std::endl
            << "   1-2:" << std::endl
            << "        - matches on \"1-2\", \"1-3\", ..." << std::endl
            << "   1-:2" << std::endl
            << "        - matches on \"1-1\" and \"1-2\"." << std::endl
            << "Matches work whether or not leading zeros are present, thus \"01-01\" is equivalent to \"1-1\"." << std::endl;
        std::cout << o.str();
        log(logfile,o.str());
}

bool SelectClause::pg_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
        if (trace_evaluate) trace_ostream << "SelectClause::pg_matches called with NULL pointer to LUN." << std::endl;
		return false;
	}

	std::string pg_string = p_LUN->attribute_value("Parity Group");

	trim(pg_string); // remove leading/trailing whitespace
	if (pg_string.size() == 0)
	{
        pg_string = p_LUN->attribute_value("PG");

        if (pg_string.size() == 0)
        {
            if (trace_evaluate) { trace_ostream << "SelectClause::pg_matches() - LUN had neither \"Parity Group\" nor \"PG\" parity_group attributes or their values are at most whitespace." << std::endl; }
            return false;
        }
	}

	int first_int, last_int;

	std::smatch bits;
	if (std::regex_match(pg_string,bits,regex_PG))
	{
        std::ssub_match first_sub = bits[1];
        std::ssub_match last_sub = bits[2];
        first_int = atoi(first_sub.str().c_str());
        last_int = atoi(last_sub.str().c_str());
	}
	else
	{
        if (trace_evaluate)
        {
            trace_ostream << "SelectClause::pg_matches() - LUN\'s parity group \"" << pg_string << "\" does not look like a parity group (digits hyphen digits)." << std::endl;
        }
       return false;
	}

	for (auto& s : attribute_value_tokens())
	{
        if (s == "*") return true;


        if (!std::regex_match(s,bits,regex_before_after_hyphen))
        {
            complain_pg(s);
            return false;
        }

        std::ssub_match before_part_match = bits[1];
        std::ssub_match after_part_match = bits[2];

        std::string before_part = before_part_match.str();
        std::string after_part = after_part_match.str();

        if ( pg_part_matches(first_int,before_part) && pg_part_matches(last_int,after_part) )
        {
            return true;
        }
	}
	return false;
}

bool SelectClause::host_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
        if (trace_evaluate) trace_ostream << "SelectClause::host_matches called with NULL pointer to LUN." << std::endl;
		return false;
	}

	std::string host_string = p_LUN->attribute_value("ivyscript_hostname");

	trim(host_string); // remove leading/trailing whitespace
	if (host_string.size() == 0)
	{
        host_string = p_LUN->attribute_value("hostname");

        if (host_string.size() == 0)
        {
            if (trace_evaluate) { trace_ostream << "SelectClause::host_matches() - LUN had neither \"ivyscript_hostname\" nor \"hostname\" parity_group attributes or their values are at most whitespace." << std::endl; }
            return false;
        }
	}

    for (auto& s : attribute_value_tokens())
    {
        if (stringCaseInsensitiveEquality(s,host_string)) return true;
    }

    return false;
}

bool matches_SelectClause_pointer_list(std::list<SelectClause*>*p, LUN* pLUN)
{
    if (p == nullptr) return false;

    for (auto& p_SelectClause : (*p))
    {
        if (p_SelectClause->matches(pLUN)) return true;
    }
    return false;
}

bool select_clause_list_contains_attribute( std::list<SelectClause*>*p, const std::string& attribute_name)
{
    if (p == nullptr) return false;

    std::string normalized_attribute_name = column_header_to_identifier(attribute_name);

    for (auto& p_clause : (*p))
    {
        if (normalized_attribute_name == p_clause->attribute_name_token()) return true;
    }
    return false;

}

void SelectClause::display(const std::string& indent, std::ostream& o)
{
    o << indent << "SelectClause at " << bookmark;
    if (p_attribute_name_Xpr == nullptr)
    {
        o << " has null pointer to attribute name expression." << std::endl;
    }
    else
    {
        o << " attribute name expression:" << std::endl;
        p_attribute_name_Xpr->display(indent+indent_increment,o);
    }

    o << indent << "SelectClause at " << bookmark << " (continued)";
    if (p_SelectClauseValue_pointer_list == nullptr)
    {
        o << " has null pointer to SelectClauseValue pointer list." << std::endl;
    }
    else
    {
        o << " SelectClauseValue pointer list has " << p_SelectClauseValue_pointer_list->size() << " entries:" << std::endl;
        for (auto& p : *p_SelectClauseValue_pointer_list)
        {
            p->display(indent+indent_increment,o);
        }
    }
}
