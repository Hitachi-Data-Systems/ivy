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
