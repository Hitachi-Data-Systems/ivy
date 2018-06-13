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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <iostream>
#include <sstream>
#include <list>

#include "LUN.h"
#include "JSON_select.h"
#include "LDEVset.h"
#include "ivyhelpers.h"
#include "ivy_engine.h"
#include "select/select.parser.hh"


typedef struct yy_buffer_state *YY_BUFFER_STATE;

extern YY_BUFFER_STATE xx__scan_string ( const char *yy_str  );
void xx__delete_buffer ( YY_BUFFER_STATE b  );

std::string JSON_select_value::bare_value()
{
    switch (t)
    {
        case JSON_select_value_type::bare_JSON_number:
            {
                if (nullptr == p_s)
                {
                    std::ostringstream o;
                    o << "<Error> JSON_select_value object is of type JSON_select_value_type::bare_JSON_number, but the pointer to the string is the null pointer."
                    << "  Occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__;
                    return o.str();
                }
                return (*p_s);
            }
        case JSON_select_value_type::quoted_string:
            {
                if (nullptr == p_s)
                {
                    std::ostringstream o;
                    o << "<Error> JSON_select_value object is of type JSON_select_value_type::quoted_string, but the pointer to the string is the null pointer."
                    << "  Occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__;
                    return o.str();
                }
                return (*p_s);
             }
        case JSON_select_value_type::true_keyword: return std::string("true");
        case JSON_select_value_type::false_keyword: return std::string("false");
        case JSON_select_value_type::null_keyword: return std::string("null");
        default:
        {
            std::ostringstream o;
            o << "<Error> Unknown JSON_select_value_type in operator<< for JSON_select_value"
            << " - in function " << __FUNCTION__ << " at line " << __LINE__ << " in file " << __FILE__;
            return o.str();
        }
    }
}

std::ostream& operator<< (std::ostream& o, const JSON_select_value& v)
{
    switch (v.t)
    {
        case JSON_select_value_type::bare_JSON_number:
            {
                if (nullptr == v.p_s)
                {
                    o << "<Error> JSON_select_value object is of type JSON_select_value_type::bare_JSON_number, but the pointer to the string is the null pointer."
                    << "  Occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__;
                    break;
                }
                o << *(v.p_s);
                break;
            }
        case JSON_select_value_type::quoted_string:
            {
                if (nullptr == v.p_s)
                {
                    o << "<Error> JSON_select_value object is of type JSON_select_value_type::quoted_string, but the pointer to the string is the null pointer."
                    << "  Occurred in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__;
                    break;
                }
                o << '\"';
                std::string& s =  *(v.p_s);
                for (unsigned int i=0; i < s.size();i++)
                {
                    auto& c = s[i];
                    switch (c)
                    {
                        case '\t': o << "\\t"; break;
                        case '\r': o << "\\r"; break;
                        case '\n': o << "\\n"; break;
                        case '\v': o << "\\v"; break;
                        default:
                        {
                            if (isprint(c)) { o << c; }
                            else            { o << "\\x" << std::hex << std::setw(2) << std::setfill('0') << c; }
                        }
                    }
                }
                o << '\"';
            }
            break;
        case JSON_select_value_type::true_keyword: o << "true"; break;
        case JSON_select_value_type::false_keyword: o << "false"; break;
        case JSON_select_value_type::null_keyword: o << "null"; break;
        default:
        {
            std::ostringstream o2;
            o2 << "unknown JSON_select_value_type in operator<< for JSON_select_value"
            << " - in function " << __FUNCTION__ << " at line " << __LINE__ << " in file " << __FILE__;
            throw std::runtime_error(o2.str());
        }
    }
    return o;
}

std::ostream& operator<< (std::ostream& o, const JSON_select_clause& Jsc)
{
    //   "attribute name" : "single value"
    // or
    //   "attribute name" : [ "list", "of", 4, "values" ]

    if (nullptr == Jsc.p_attribute_name) o << "<JSON_select_clause null pointer to attribute name>";
    else                                 o << '\"' << *Jsc.p_attribute_name << '\"';

    o << " : ";

    if (nullptr == Jsc.p_value_pointer_list)
    {
        o << "<JSON_select_clause null pointer to attribute value pointer list>";
        return o;
    }

    auto values = Jsc.p_value_pointer_list->size();

    if (0 == values)
    {
        o << "<JSON_select_clause attribute value pointer list is empty>";
        return o;
    }

    if (1 < values) o << "[ ";

    bool need_comma = false;
    for (auto& p : (*Jsc.p_value_pointer_list))
    {
        if (need_comma) { o << ", "; }
        need_comma = true;

        if (nullptr==p) { o << "<JSON_select_clause attribute value pointer is null pointer.>";}
        else
        {
            o << *p;
        }
    }

    if (1 < values) o << " ]";

    return o;
}

std::ostream& operator<< (std::ostream& o, const JSON_select& Js)
{
    auto values = Js.JSON_select_clause_pointer_list.size();

    if (0 == values)
    {
        o << "<JSON_select JSON_select_clause pointer list is empty>";
        return o;
    }

    o << "{ ";

    bool need_comma {false};
    for (auto& p : Js.JSON_select_clause_pointer_list)
    {
        if (need_comma) { o << ", "; }
        need_comma=true;

        o << (*p);
    }

    o << " }";

    return o;
}

std::string squashed_attribute_name(const std::string& s)
{
    std::string r;
    for (unsigned int i = 0; i < s.size(); i++)
    {
        auto& c = s[i];
        if (' ' == c || '_' == c) continue;
        r.push_back(tolower(c));
    }
    return r;
}

bool squashed_equivalence(const std::string& s1, const std::string& s2)
{
    return (0 == squashed_attribute_name(s1).compare(squashed_attribute_name(s2)));
}

bool JSON_select_clause::matches(LUN* p_LUN)
{
	if (nullptr == p_attribute_name) return false;

	std::string& attribute_name = *p_attribute_name;

    if (squashed_equivalence(std::string("LDEV"),attribute_name)) return ldev_matches(p_LUN);
    if (squashed_equivalence(std::string("host"),attribute_name)) return host_matches(p_LUN);
    if (squashed_equivalence(std::string("PG"),  attribute_name)) return pg_matches  (p_LUN);
    return default_matches(p_LUN);
}


bool JSON_select_clause::default_matches(LUN* pLUN)
{
    if ( pLUN == nullptr)
    {
        std::ostringstream o;
        o << "<Error> JSON_select_clause::default_matches(LUN* pLUN) - pLUN == nullptr" << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        throw std::runtime_error(o.str());
    }

    if ( p_attribute_name == nullptr)
    {
        std::ostringstream o;
        o << "<Error> JSON_select_clause::default_matches() - p_attribute_name == nullptr" << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        throw std::runtime_error(o.str());
    }
    std::string& attribute_name = *p_attribute_name;

    if ( p_value_pointer_list == nullptr)
    {
        std::ostringstream o;
        o << "<Error> JSON_select_clause::default_matches() - p_value_pointer_list == nullptr" << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        throw std::runtime_error(o.str());
    }
    std::list<JSON_select_value*>& value_pointer_list = *p_value_pointer_list;

    std::string lun_attribute_value = pLUN->attribute_value(attribute_name);

    for (auto& p : value_pointer_list) // return true if it matches one of the target values
    {
        if (p == nullptr)
        {
            std::ostringstream o;
            o << "<Error> JSON_select_clause::default_matches() - a null pointer was found in the list of pointers to target attribute values" << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
            throw std::runtime_error(o.str());
        }

        if (stringCaseInsensitiveEquality(p->bare_value(),lun_attribute_value))
        {
            return true;
        };
    }

    return false;
}


bool JSON_select_clause::ldev_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
		return false;
	}

	if (nullptr == p_value_pointer_list)
	{
        std::ostringstream o;
        o << "<Warning> JSON_select_clause has null pointer to list of attribute value pointers." << std::endl << "Select clause is:  " << (*this) << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
		return false;
	}
    std::list<JSON_select_value*>& value_pointer_list = *p_value_pointer_list;

	std::string lun_ldev_string = p_LUN->attribute_value("LDEV");

	trim(lun_ldev_string); // remove leading/trailing whitespace

	if (lun_ldev_string.size() == 0)
	{
        //if (trace_evaluate) { trace_ostream << "JSON_select_clause::ldev_matches() - LUN did not have the LDEV attribute or its value was at most whitespace." << std::endl; }
        return false;
	}

    int lun_ldev_int = LDEVset::LDEVtoInt(lun_ldev_string);

	if (-1 == lun_ldev_int)
	{
        std::ostringstream o;
        o << "<Warning> JSON_select_clause::ldev_matches() - LUN LDEV attribute value \"" << lun_ldev_string << "\" did not parse as an LDEV." << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
		return false;
	}

    LDEVset ls {};

    for (auto&  p : value_pointer_list)
    {
        if (nullptr == p)
        {
            std::ostringstream o;
            o << "<Warning> In JSON_select_clause::ldev_matches() at line " << __LINE__ << " of source file " << __FILE__
                << ", ignoring null pointer in list of pointers to attribute values." << std::endl
                << "Select clause is:  " << (*this) << std::endl;
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
            continue;
        }

        std::string ldev_set_token = p->bare_value();

        trim(ldev_set_token);

        if (ldev_set_token.size() == 0) continue;

        if (!ls.add(ldev_set_token,m_s.masterlogfile))
        {
            std::cout << "[Select] clause LDEV shorthand match failure - LDEV match specification \"" << ldev_set_token << "\" did not parse as an LDEVset." << std::endl;

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
        std::ostringstream o; o << __FILE__ << " line " << __LINE__ << "" << __FUNCTION__ << " - this is not supposed to be able to happen.\n";
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
        throw std::runtime_error(o.str());
    }
    return false; // unreachable - put this here to stop compiler messate
}


void JSON_select_clause::complain_pg(std::string s)
{
        std::ostringstream o;
        o << "JSON_select_clause - PG match specification \"" << s << "\" is invalid." << std::endl
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
        log(m_s.masterlogfile,o.str());
}

bool JSON_select_clause::pg_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
        std::ostringstream o;
        o << "<Error> at " <<  __FILE__ << " line " << __LINE__ << " function " << __FUNCTION__ << " called with h NULL pointer to LUN." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
		return false;
	}

	if (nullptr == p_value_pointer_list)
	{
        std::ostringstream o;
        o << "<Warning> In function " << __FUNCTION__ << " in file " << __FILE__ << " at line " << __LINE__
            << " - null pointer to list of attribute value pointers.  Select clause is " << (*this);
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
		return false;
	}
    std::list<JSON_select_value*>& value_pointer_list = *p_value_pointer_list;

	std::string pg_string = p_LUN->attribute_value("Parity Group");

	trim(pg_string); // remove leading/trailing whitespace
	if (pg_string.size() == 0)
	{
        pg_string = p_LUN->attribute_value("PG");

        if (pg_string.size() == 0)
        {
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
        std::ostringstream o; o << "<Warning> At " << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__
            << " - LUN has a parity group attribute, but its value \"" << pg_string << "\" does not look like a parity group (digits hyphen digits)." << std::endl;
        log(m_s.masterlogfile, o.str());
        return false;
	}

	for (auto& p : value_pointer_list)
	{
        if (nullptr==p)
        {
            std::ostringstream o; o << "<Warning> At " << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << " - "
                << " ignoring a null pointer in the list of pointers to value strings." << std::endl;
            log(m_s.masterlogfile, o.str());
            continue;
        }

        std::ostringstream q;

        std::string s = p->bare_value();

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



bool JSON_select_clause::host_matches(LUN* p_LUN)
{
	if (nullptr == p_LUN)
	{
        std::ostringstream o;
        o << "<Warning> JSON_select_clause::host_matches called with NULL pointer to LUN." << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
        log(m_s.masterlogfile, o.str());
		return false;
	}

	if (nullptr == p_value_pointer_list)
	{
        std::ostringstream o;
        o << "<Error> In function " << __FUNCTION__ << " in file " << __FILE__ << " at line " << __LINE__
            << " - null pointer to list of attribute value pointers.  Select clause is " << (*this);
        log(m_s.masterlogfile, o.str());
		return false;
	}
    std::list<JSON_select_value*>& value_pointer_list = *p_value_pointer_list;

    std::list<std::string> expanded_hostnames;

    std::regex host_range_regex(   R"((([[:alpha:]][[:alpha:]_]*)|([[:alpha:]][[:alnum:]_]*[[:alpha:]_]))([[:digit:]]+)-([[:digit:]]+))"    );   // e.g. "cb32-47" or even "host2a_32-47"
    std::regex single_hostname_regex(   R"([[:alpha:]][[:alnum:]_]*)"    );
    std::regex dotted_quad_regex  (  R"ivy((([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5])))ivy");

	for (auto& p : value_pointer_list)
	{
        if (nullptr==p)
        {
            std::ostringstream o; o << "<Warning> At " << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << " - "
                << " ignoring a null pointer in the list of pointers to values." << std::endl;
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
            continue;
        }

        std::string s = p->bare_value();
        //trim(s);

        if (std::regex_match(s,single_hostname_regex) || std::regex_match(s,dotted_quad_regex))
        {
            expanded_hostnames.push_back(s);
            continue;
        }

        std::smatch smash;
        std::ssub_match sub;

        if (std::regex_match(s,smash,host_range_regex))
        {
            std::string hostname_base, hostname_range_start, hostname_range_end;
            sub = smash[1]; hostname_base = sub.str();
            sub = smash[4]; hostname_range_start = sub.str();
            sub = smash[5]; hostname_range_end = sub.str();

            unsigned int range_start;  {std::istringstream xx(hostname_range_start); xx >> range_start;}
            unsigned int range_end;    {std::istringstream xy(hostname_range_end);   xy >> range_end;}

            if (range_end <= range_start)
            {
                std::ostringstream o; o << "<Warning> At " << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << " - "
                    << " ignoring select clause host range specification \"" << s << "\" where the starting host comes after the ending host." << std::endl;
                log(m_s.masterlogfile, o.str());
                std::cout << o.str();
                continue;
            }
            for (unsigned int i = range_start; i <= range_end; i++)
            {
                std::ostringstream o;
                o << hostname_base << i;
                expanded_hostnames.push_back(o.str());
            }
            continue;
        }

        {
            std::ostringstream o; o << "<Warning> At " << __FILE__ << " line " << __LINE__ << " in function " << __FUNCTION__ << " - "
                << " ignoring a select clause host attribute value \"" << s
                << "\" that doesn\'t look like a single hostname like shovel or a hostname range like shovel32-47 or an IPV4 dotted quad like 192.168.0.0." << std::endl;
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
        }
    }

	std::string host_string = p_LUN->attribute_value("ivyscript_hostname");

	trim(host_string); // remove leading/trailing whitespace
	if (host_string.size() == 0)
	{
        host_string = p_LUN->attribute_value("hostname");

        if (host_string.size() == 0)
        {
            std::ostringstream o;
            o << "<Warning> JSON_select_clausehost_matches() - LUN had neither \"ivyscript_hostname\" nor \"hostname\" parity_group attributes or their values are at most whitespace" << std::endl
            << "in function " << __FUNCTION__ << " at line " << __LINE__ << " of file " << __FILE__ << std::endl;
            log(m_s.masterlogfile, o.str());
            std::cout << o.str();
            return false;
        }
	}

    for (auto& s : expanded_hostnames)
    {
        if (stringCaseInsensitiveEquality(s,host_string)) return true;
    }

    return false;
}


bool JSON_select::matches(LUN* pLUN)
{
    for (auto& p : JSON_select_clause_pointer_list)
    {
        if ((nullptr==p) || (!p->matches(pLUN))) return false;
    }

    return true;
}

bool JSON_select::contains_attribute(const std::string& a)
{
    for (auto& p : JSON_select_clause_pointer_list)
    {
        if (nullptr !=p)
        {
            if (p->p_attribute_name != nullptr)
            {
                std::string& attribute_name = *(p->p_attribute_name);
                if (stringCaseInsensitiveEquality(a,attribute_name)) return true;
            }
        }
    }
    return false;
}

void JSON_select::clear()
{
    for (auto& p : JSON_select_clause_pointer_list) delete p;
                   JSON_select_clause_pointer_list.clear();

    successful_compile = false;
    unsuccessful_compile = false;

    error_message.clear();

    return;

}

bool JSON_select::compile_JSON_select(const std::string& s)
{
    clear();

    if (s.size() == 0)
    {
        successful_compile = true;
        error_message = "select expression evaluated to null string";
        return true;
    }

	xx_::select parser(*this);

    YY_BUFFER_STATE xx_buffer = xx__scan_string(s.c_str());  // it's two underscores, the first is part of the {xx_} prefix

	parser.parse();

    xx__delete_buffer(xx_buffer);

	if (successful_compile)
	{
        std::ostringstream o;
        o << "Successful compile";
        error_message = o.str();
        return true;
	}

    error_message += "Unsuccessful compile.";

    return false;
}




