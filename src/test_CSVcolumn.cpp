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

#include <iostream>
#include <iomanip>

#include "ivyhelpers.h"

void for_string(std::string csvline)
{
	std::cout << std::endl << std::endl << "For csvline=\"" << csvline << "\":" << std::endl;
	std::cout << "    countCSVlineUnquotedCommas(csvline) = " << countCSVlineUnquotedCommas(csvline)<< std::endl;
	for (int i=0 ; i<5 ; i++)
	{
		std::cout << "   retrieveRawCSVcolumn(\"" << csvline << "\", " << i << ") = \"" << retrieveRawCSVcolumn(csvline,i) << "\"." << std::endl; 
		std::cout << "   unwrapped CSV column(\"" << csvline << "\", " << i << ") = \"" << UnwrapCSVcolumn(retrieveRawCSVcolumn(csvline,i)) << "\"." << std::endl; 
		std::cout << std::endl;
	}
}

int main(int argc, char* argv[])
{
	std::cout << "Hello, whirrled!" << std::endl;
	for_string(std::string("\",\""));

	for_string(std::string(""));
	for_string(std::string(","));
	for_string(std::string("a"));
	for_string(std::string("a,"));
	for_string(std::string("zero"));
	for_string(std::string("a,z"));
	for_string(std::string(",one"));
	for_string(std::string("zero,one"));
	for_string(std::string("zero,one,"));
	for_string(std::string("zero,one,,"));
	for_string(std::string("zero,one,,three"));
	for_string(std::string("zero,one,two,three"));
	for_string(std::string("zero,one"));
	for_string(std::string("zero,one,,,four"));
	for_string(std::string(",,,,four"));
	
	for_string(std::string(""));
	for_string(std::string("\",\""));
	for_string(std::string("a"));
	for_string(std::string("a,"));
	for_string(std::string("zero"));
	for_string(std::string("a,z"));
	for_string(std::string(",one"));
	for_string(std::string("zero,\'one\'"));
	for_string(std::string("zero,one,"));
	for_string(std::string("zero,  one   ,,"));
	for_string(std::string("zero,  \"one \"  ,,three"));
	for_string(std::string("zero,  \"one \" \"more_one\"  ,,three"));
	for_string(std::string("zero,one,  two,three"));
	for_string(std::string("zero,one"));
	for_string(std::string("zero,one,,,four"));
	for_string(std::string(",,,,four"));
}
