//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
