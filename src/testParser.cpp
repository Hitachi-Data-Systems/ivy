//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
//testSelect.cpp

#include <iostream>

#include "../../ivytime/lib/ivytime.h"
#include "ivyhelpers.h"
#include "NameEqualsValueList.h"  // JSON on geritol
#include "ListOfNameEqualsValueList.h"


int main(int argc, char* argv[])
{

	std::list<std::string> datalines 
	{
		R"raw(LDEV = 00:00-00:FF, Port = {1A,3A,5A,7A}  PG={ 1:3-*, 15-15 })raw",


		R"raw(LDEV = 00:00)raw",
		R"raw(LDEV = "00:00, 01:00-01:FF")raw",
		R"raw(LDEV = {00:00, 01:00-01:FF})raw",
		R"raw(LDEV = {00:00 01:00-01:FF})raw",
		R"raw(LDEV={ 00:00 01:00-01:FF })raw",
		R"raw(LDEV = 00:00-00:FF, Port = {1A,3A,5A,7A}  PG={ 1:3-*, 15-15 })raw",
		R"raw(host="sun159 cb28-31")raw",
		R"raw(host = {sun159 cb28-31})raw",
		R"raw(serial_number+Port = 410123+1A)raw",
		R"raw(!)raw",
		R"raw(name =)raw",
		R"raw(name ={)raw",
		R"raw(name = { a)raw",
		R"raw(name = !@#%$^&&*())raw",


		R"raw()raw" /* , */
	};

	ListOfNameEqualsValueList lonevl;
	for (auto& s: datalines)
	{
		std::cout << std::endl << std::endl << "Before lonevl.set(" << s << ")" << std::endl;
		lonevl.set(s);
		std::cout << std::endl << std::endl << "After lonevl.set(" << s << ")" << std::endl;
		std::cout << std::endl << lonevl.toString() << std::endl; 
	}
	return 0;
}
