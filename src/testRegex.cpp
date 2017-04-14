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
#include <regex>
#include <iostream>

#include "ivytime.h"
#include "ivyhelpers.h"
#include "ivydefines.h"

std::regex MMSS_regex( std::string( R"ivy(([[:digit:]]+):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of minutes followed by ':' followed by 00-59 or 0-59.
std::regex HHMMSS_regex( std::string( R"ivy(([[:digit:]]+):(([012345])?[[:digit:]]):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of hours followed by ':' followed by 00-59 or 0-59 followed by ':' followed by 00-59 or 0-59.
std::regex has_trailing_fractional_zeros_regex( R"((([[:digit:]]+)\.([[:digit:]]*[1-9])?)0+)" );

std::string remove_trailing_fractional_zeros(std::string s)
{
    std::smatch entire_match;
    std::ssub_match before, after, before_point_after;

    if (std::regex_match(s, entire_match, has_trailing_fractional_zeros_regex))
    {
        before_point_after = entire_match[1];
        before = entire_match[2];
        after = entire_match[3];

        if (0 == after.str().size())
        {
            return before.str();  // We remove the decimal point if there are only zeros after it.
                                  // This does change it from a floating point literal to an unsigned int literal.
        }
        else
        {
            return before_point_after.str();
        }
    }
    else
    {
        return s;
    }
}

std::string rewrite_HHMMSS_to_seconds( std::string s )
{
        std::smatch entire_match;
        std::ssub_match hh, mm, ss, fractional_part;



        if (std::regex_match(s, entire_match, MMSS_regex))
        {
            mm = entire_match[1];
            ss = entire_match[2];
            std::istringstream imm(mm.str());
            std::istringstream iss(ss.str());
            double m, s;
            imm >> m;
            iss >> s;
            std::ostringstream o;
            o << std::fixed << (s+(60*m));
            return o.str();
        }

        if (std::regex_match(s, entire_match, HHMMSS_regex))
        {
            hh = entire_match[1];
            mm = entire_match[2];
            ss = entire_match[4];
            std::istringstream ihh(hh.str());
            std::istringstream imm(mm.str());
            std::istringstream iss(ss.str());
            double h, m, s;
            ihh >> h;
            iss >> s;
            imm >> m;
            std::ostringstream o;
            o << std::fixed << (s+(60*(m+(60*h))));
            return o.str();
        }

        return s;
}

int main(int argc, char* argv[])
{
    std::cout << "Hello, whirrled!" << std::endl;

    std::vector<std::string> ssss
    {
        "0", "0:0", "0:00", "0:000", "0:59", "0:60", "30", "30:0", "30:00", "30:000", "30:59", "30:60"
        , "0:0:0", "1:0:0", "1:00:00", "1:1:1", "0:00:59", "0:59:00"
        , "0.", "0:0.", "0:00.", "0:000.", "0:59.", "0:60.", "30.", "30:0.", "30:00.", "30:000.", "30:59.", "30:60."
        , "0:0:0.", "1:0:0.", "1:00:00.", "1:1:1.", "0:00:59.", "0:59:00."
        , "0.1", "0:0.1", "0:00.1", "0:000.1", "0:59.1", "0:60.1", "30.1", "30:0.1", "30:00.1", "30:000.1", "30:59.1", "30:60.1"
        , "0:0:0.1", "1:0:0.1", "1:00:00.1", "1:1:1.1", "0:00:59.1", "0:59:00.1"
    };

    for (auto& s : ssss)
    {
        std::cout << "Rewrite of \"" << s << "\" is \"" << rewrite_HHMMSS_to_seconds(s) << "\", which strips trailing zeros to \"" << remove_trailing_fractional_zeros(rewrite_HHMMSS_to_seconds(s)) << "\".\n\n";

    }






//	std::regex identifier_regex( R"([a-zA-Z]\w*)" );
//	std::regex number_regex( R"((([0-9]+(\.[0-9]*)?)|(\.[0-9]+))%?)" );
//
//    std::string subinterval_duration_as_string {"<5,0>;1.2;2.5"};
//
//    std::regex regex_split_to_3_at_semicolons ("([^;]+);([^;]+);([^;]+)");
//
//
//	for (std::string s : { "subsystem", "124", "-1", "1e6", "-1e6", "1e+34", "-1e-34", "-.1", "-1.1", ".56%", "555%", "5.5%", "%" } )
//	// "a_56", "5.6", ".56", "56.", "subsystem", "5.6.", "henry", "h_56", "ha_56", "ha", "ha_", "subsystem1", "subsystem_1",
//
//	{
//		std::cout << "For string \"" << s << "\" ";
//		if (std::regex_match(s,identifier_regex))
//		{
//			std::cout << "which is an identier ";
//		}
//		else if (std::regex_match(s,number_regex))
//		{
//			std::cout << "which is a number";
//		}
///*debug*/	else
//		{
//			std::cout<< "which is neither an identifier nor a number ";
//		}
//		std::cout << std::endl << std::endl;
//
//		if (std::regex_match(s,float_number_optional_trailing_percent_regex))
//		{
//            std::cout << "\"" << s << "\" matches float_number_optional_trailing_percent." << std::endl;
//		}
//		else
//		{
//            std::cout << "\"" << s << "\" does not match float_number_optional_trailing_percent." << std::endl;
//		}
//	}
//





//    std::regex host_range_regex(   R"((([[:alpha:]][[:alpha:]_]*)|([[:alpha:]][[:alnum:]_]*[[:alpha:]_]))([[:digit:]]+)-([[:digit:]]+))"    );   // e.g. "cb32-47" or even "host2a_32-47"
//    std::smatch smash;
//    std::ssub_match sub;
//
//    for (std::string s : {"cb56-345", "cbkk3aaa32-47"})
//    {
//        std::cout << "For string \"" << s << "\", ";
//
//        std::string hostname_base, hostname_range_start, hostname_range_end;
//
//        if (std::regex_match(s,smash,host_range_regex))
//        {
//            sub = smash[1]; hostname_base = sub.str();
//            sub = smash[4]; hostname_range_start = sub.str();
//            sub = smash[5]; hostname_range_end = sub.str();
//
//            std::cout << "hostname_base = \"" << hostname_base <<"\", hostname_range_start = \"" << hostname_range_start << "\". hostname_range_end = \"" << hostname_range_end << "\"." << std::endl;
//            std::cout << "matched on host name range regex with " << smash.size() << " submatches." << std::endl;
//            for (unsigned int i = 0; i < smash.size(); i++)
//            {
//                sub = smash[i];
//                std::string s = sub.str();
//                std::cout << "submatch " << i << " is \"" << s << "\" (" << s.size() << ")" << std::endl;
//            }
//        }
//        else
//        {
//            std::cout << "did not match on host name range regex." << std::endl;
//        }
//
//    }




	return 0;
}
