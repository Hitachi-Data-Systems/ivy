//Copyright (c) 2015-2020 Hitachi Vantara Corporation
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

#include <string>
#include <cctype>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <list>
#include <algorithm>    // std::find_if
#include <regex>

using ivy_float = double;

bool stringCaseInsensitiveEquality(std::string s1, std::string s2) {
        if (s1.length()!=s2.length()) return false;
        if (s1.length()==0) return true;
        for (int i=0; i<s1.length(); i++) if (toupper(s1[i]) != toupper(s2[i])) return false;
        return true;
}

bool rewrite_total_IOPS(std::string& parametersText, int command_workload_IDs)
{
	// rewrites the first instance ("total_IOPS = 5000", 5) => "IOPS=1000".
	// returns true if it re-wrote, false if it didn't see "total_IOPS"

	if (command_workload_IDs < 0) throw (std::invalid_argument("rewrite_total_IOPS() command_workload_IDs must be greater than zero."));

	std::string total_IOPS {"total_IOPS"};

	if (total_IOPS.length() > parametersText.length()) return false;
	
	int start_point, number_start_point, last_plus_one_point;

	for (int i=0; i < (parametersText.length()-(total_IOPS.length()-1)); i++)
	{

//std::cout << "top of loop - parametersText=\"" << parametersText << "\", parametersText.length()=" << parametersText.length() 
//<< ", total_IOPS=\"" << total_IOPS << "\", total_IOPS.length()=" << total_IOPS.length() 
//<< ", i=" << i << ",  (parametersText.length()-(total_IOPS.length()-1)=" << (parametersText.length()-(total_IOPS.length()-1) << std::endl;

		if (stringCaseInsensitiveEquality(total_IOPS,parametersText.substr(i,total_IOPS.length())))
		{
			start_point=i;
			i += total_IOPS.length();
			
			while (i < parametersText.length()  && isspace(parametersText[i])) i++;
			
			if (i < parametersText.length()  && '=' == parametersText[i])
			{
				i++;
				while (i < parametersText.length()  && isspace(parametersText[i])) i++;

				number_start_point=i;
				
				// number forms recognized:  1 .2 3. 4.4 1.0e3 1e+3 1.e-3 3E4
				
				if 
				((
					i < parametersText.length() && isdigit(parametersText[i])
				) || (
					i < (parametersText.length()-1) && '.' == parametersText[i] && isdigit(parametersText[i+1])						
				))
				{
					// we have found a "total_IOPS = 4.5" that we will edit
					bool have_decimal_point {false};
					while (i < parametersText.length())
					{
						if (isdigit(parametersText[i]))
						{
							i++; 
							continue;
						}
						if ('.' == parametersText[i])
						{
							if (have_decimal_point) break;
							i++;
							have_decimal_point=true;
							continue;
						}
						break;
					}
					
					if ( i < (parametersText.length()-1) && ('e' == parametersText[i] || 'E' == parametersText[i]) && isdigit(parametersText[i+1]))
					{
						i+=2;
						while (i < parametersText.length() && isdigit(parametersText[i])) i++;
					}
					else if ( i < (parametersText.length()-2) && ('e' == parametersText[i] || 'E' == parametersText[i]) && ('+' == parametersText[i+1] || '-' == parametersText[i+1]) && isdigit(parametersText[i+2]))
					{
						i+=3;
						while (i < parametersText.length() && isdigit(parametersText[i])) i++;
					}
					last_plus_one_point=i;
//*debug*/std::cout<<"i="<<i<<", start_point="<<start_point<<", number_start_point="<<number_start_point<<", last_plus_one_point="<<last_plus_one_point<<", have_decimal_point="<<have_decimal_point<<std::endl;
//*debug*/ {std::cout << "value string =\"" << parametersText.substr(number_start_point,last_plus_one_point-number_start_point) << "\"." << std::endl;}
//*debug*/ {std::cout << "master_stuff.cpp rewrite_total_IOPS() - before \"" << parametersText << "\"." << std::endl;}
					std::istringstream isn(parametersText.substr(number_start_point,last_plus_one_point-number_start_point));
					ivy_float value;
					isn >> value;
//*debug*/ {std::cout << "master_stuff.cpp rewrite_total_IOPS() - value = " << value << std::endl;}
					value /= (ivy_float) command_workload_IDs;
					std::ostringstream o; o << "IOPS=" << std::fixed << std::setprecision(12) << value;
					parametersText.replace(start_point,last_plus_one_point-start_point,o.str());
					return true;
//*debug*/ {std::cout << "master_stuff.cpp rewrite_total_IOPS() - after \"" << parametersText << "\"." << std::endl;}
				}
			}
		}
//std::cout << "bottom of loop - parametersText=\"" << parametersText << "\", parametersText.length()=" << parametersText.length() 
//<< ", total_IOPS=\"" << total_IOPS << "\", total_IOPS.length()=" << total_IOPS.length() 
//<< ", i=" << i << ",  (parametersText.length()-(total_IOPS.length()-1)=" << (parametersText.length()-(total_IOPS.length()-1) << std::endl;
	}
	return false; // return point if no match.
}

void trythis (const char* p, int n)
{
	std::string s{p};
	std::cout << "original = \"" << s << "\"." << std::endl;
	rewrite_total_IOPS(s, n);
	std::cout << "edited   = \"" << s << "\"." << std::endl << std::endl;
	return;
}
			
int main(int arc, char* argv[])
{
	trythis("bork", 3);
	trythis("bork=5, total_iops=300, some other", 3);
	trythis("total_iops=300, some other", 3);
	trythis("bork=5, total_iops=300", 3);
	trythis("total_iops=300", 3);
	trythis("total_iops=.3", 3);
	trythis("total_iops=300.", 3);
	trythis("total_iops=.300.", 3);
	trythis("total_IOPS=1e3", 3);
	trythis("total_IOPS=1.3e3", 3);
	trythis("total_IOPS=.3e3", 3);
	trythis("total_IOPS=1E3", 3);

	trythis("total_IOPS=1e+3", 3);
	trythis("total_IOPS=1.3e-3", 3);
	

}
