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

#include "ivytypes.h"
#include "ivyhelpers.h"

//std::regex MMSS_regex( std::string( R"ivy(([[:digit:]]+):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of minutes followed by ':' followed by 00-59 or 0-59.
//std::regex HHMMSS_regex( std::string( R"ivy(([[:digit:]]+):(([012345])?[[:digit:]]):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of hours followed by ':' followed by 00-59 or 0-59 followed by ':' followed by 00-59 or 0-59.
//std::regex has_trailing_fractional_zeros_regex( R"((([[:digit:]]+)\.([[:digit:]]*[1-9])?)0+)" );
//
//std::string remove_trailing_fractional_zeros(std::string s)
//{
//    std::smatch entire_match;
//    std::ssub_match before, after, before_point_after;
//
//    if (std::regex_match(s, entire_match, has_trailing_fractional_zeros_regex))
//    {
//        before_point_after = entire_match[1];
//        before = entire_match[2];
//        after = entire_match[3];
//
//        if (0 == after.str().size())
//        {
//            return before.str();  // We remove the decimal point if there are only zeros after it.
//                                  // This does change it from a floating point literal to an unsigned int literal.
//        }
//        else
//        {
//            return before_point_after.str();
//        }
//    }
//    else
//    {
//        return s;
//    }
//}
//
//std::string rewrite_HHMMSS_to_seconds( std::string s )
//{
//        std::smatch entire_match;
//        std::ssub_match hh, mm, ss, fractional_part;
//
//
//
//        if (std::regex_match(s, entire_match, MMSS_regex))
//        {
//            mm = entire_match[1];
//            ss = entire_match[2];
//            std::istringstream imm(mm.str());
//            std::istringstream iss(ss.str());
//            double m, s;
//            imm >> m;
//            iss >> s;
//            std::ostringstream o;
//            o << std::fixed << (s+(60*m));
//            return o.str();
//        }
//
//        if (std::regex_match(s, entire_match, HHMMSS_regex))
//        {
//            hh = entire_match[1];
//            mm = entire_match[2];
//            ss = entire_match[4];
//            std::istringstream ihh(hh.str());
//            std::istringstream imm(mm.str());
//            std::istringstream iss(ss.str());
//            double h, m, s;
//            ihh >> h;
//            iss >> s;
//            imm >> m;
//            std::ostringstream o;
//            o << std::fixed << (s+(60*(m+(60*h))));
//            return o.str();
//        }
//
//        return s;
//}
//
//std::string get_my_path_part()
//{
//#define MAX_FULLY_QUALIFIED_PATHNAME 511
//    const size_t pathname_max_length_with_null = MAX_FULLY_QUALIFIED_PATHNAME + 1;
//    char pathname_char[pathname_max_length_with_null];
//
//    // Read the symbolic link '/proc/self/exe'.
//    const char *proc_self_exe = "/proc/self/exe";
//    const int readlink_rc = int(readlink(proc_self_exe, pathname_char, MAX_FULLY_QUALIFIED_PATHNAME));
//
//    std::string fully_qualified {};
//
//    if (readlink_rc <= 0) { return ""; }
//
//    fully_qualified = pathname_char;
//
//    std::string path_part_regex_string { R"ivy((.*/)([^/]+))ivy" };
//    std::regex path_part_regex( path_part_regex_string );
//
//    std::smatch entire_match;
//    std::ssub_match path_part;
//
//    if (std::regex_match(fully_qualified, entire_match, path_part_regex))
//    {
//        path_part = entire_match[1];
//        return path_part.str();
//    }
//
//    return "";
//}



int main(int argc, char* argv[])
{

//    std::cout << "Hello world! my name is \"" << argv[0] << "\" and the path to my executable is \"" << get_my_path_part() << "\"" << std::endl << std::endl;


    std::string prompt {}, prompt1{R"(Hello, whirrled! from cb38<=|=>[serial number = 460064; unit identifier = HM800; microcode version = 83-04-01/00; RMLIB version = 01-23-03/00; ivy_cmddev version = 1.00.00])"};
    std::cout << format_utterance ("original prompt ", prompt1) << std::endl;

    std::regex whirrled_regex( R"((.+)<=\|=>(.+))");

    std::smatch entire_whirrled;
    std::ssub_match whirrled_first, whirrled_last;

    std::string whirrled_version {};

    if (std::regex_match(prompt1, entire_whirrled, whirrled_regex))
    {
        whirrled_first = entire_whirrled[1];
        whirrled_last  = entire_whirrled[2];

        prompt           = whirrled_first.str();
        whirrled_version = whirrled_last.str();
        trim(whirrled_version);

        std::cout << format_utterance("trimmed prompt ", prompt) << std::endl;
        std::cout << format_utterance("whirrled_version ", whirrled_version) << std::endl;


    }





//    std::cout << "Hello, whirrled!" << std::endl;


//    std::vector<std::string> ssss
//    {
//        "0", "0:0", "0:00", "0:000", "0:59", "0:60", "30", "30:0", "30:00", "30:000", "30:59", "30:60"
//        , "0:0:0", "1:0:0", "1:00:00", "1:1:1", "0:00:59", "0:59:00"
//        , "0.", "0:0.", "0:00.", "0:000.", "0:59.", "0:60.", "30.", "30:0.", "30:00.", "30:000.", "30:59.", "30:60."
//        , "0:0:0.", "1:0:0.", "1:00:00.", "1:1:1.", "0:00:59.", "0:59:00."
//        , "0.1", "0:0.1", "0:00.1", "0:000.1", "0:59.1", "0:60.1", "30.1", "30:0.1", "30:00.1", "30:000.1", "30:59.1", "30:60.1"
//        , "0:0:0.1", "1:0:0.1", "1:00:00.1", "1:1:1.1", "0:00:59.1", "0:59:00.1"
//    };
//
//    for (auto& s : ssss)
//    {
//        std::cout << "Rewrite of \"" << s << "\" is \"" << rewrite_HHMMSS_to_seconds(s) << "\", which strips trailing zeros to \"" << remove_trailing_fractional_zeros(rewrite_HHMMSS_to_seconds(s)) << "\".\n\n";
//
//    }
//





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
