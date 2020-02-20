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

#include "../../ivytime/lib/ivytime.h"
#include "ivyhelpers.h"
#include "ParameterValueLookupTable.h"

int main(int argc, char* argv[])
{
	ParameterValueLookupTable t;

	std::string f;

	f=std::string(" a=b, c=\"d e\" e=\'f%\',, aa=aardvark");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;
	
	std::cout << "t.contains(\"a\")=" << t.contains("a") << std::endl;
	std::cout << "t.contains(\"x\")=" << t.contains("x") << std::endl;


	std::cout << std::endl << std::endl;
	std::string v("a, c, e aa");
	std::cout << "t.containsOnlyValidParameterNames(\"" << v << "\",)=" << t.containsOnlyValidParameterNames(v,std::cout) << std::endl;

	std::cout << std::endl << std::endl;
	v=std::string("a, c, aa");
	std::cout << "t.containsOnlyValidParameterNames(\"" << v << "\",)=" << t.containsOnlyValidParameterNames(v,std::cout) << std::endl;
	 
	std::cout << std::endl << std::endl;
	v=std::string("a, c%, aa");
	std::cout << "t.containsOnlyValidParameterNames(\"" << v << "\",)=" << t.containsOnlyValidParameterNames(v,std::cout) << std::endl;
	 
	std::cout << std::endl << std::endl;

	f=std::string(" a=b, %net");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;


	f=std::string(" a=b, net");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;

	f=std::string(" a=b, net = ");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;

	f=std::string(" a=b, net = \"");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;


	f=std::string(" a=b, net = \'");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;


	f=std::string(" a=b, net = 2%");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;

	f=std::string(" a=b, net = %2");
	std::cout << "t.fromString(\"" << f << "\",std::cout)=" << t.fromString(f, std::cout) << std::endl;
	std::cout << "t.toString=\"" << t.toString() << "\"." << std::endl << std::endl;

	return 0;
}
