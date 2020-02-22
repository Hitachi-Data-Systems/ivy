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

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "IvyscriptLine.h"

void describe(IvyscriptLine& l)
{
	std::string my_error_message;
	
	std::cout << "IvyscriptLine \"" << l.getText() << "\"" << std::endl;
	std::cout << "isNull() = "; if (l.isNull()) std::cout << "true"; else std::cout << "false"; std::cout << std::endl;
	std::cout << "isWellFormed() = "; if (l.isWellFormed()) std::cout << "true"; else std::cout << "false - " <<std::endl << l.getParseErrorMessage(); std::cout << std::endl;
	std::cout << "has " << l.numberOfSections() << " sections." << std::endl;
	std::string section, text;
	for (int i = 0; i<l.numberOfSections() ; i++)
	{
		std::cout << "section number " << i << ' ';
		if (l.getSectionNameBySubscript(my_error_message,i,section))
		{
			std::cout << "section name = \"" << section << "\" ";
		}
		else
		{
			std::cout << "getSectionNameBySubscript() failed - " << section << std::endl;
		}
		if (l.getSectionTextBySubscript(my_error_message,i,section))
		{
			std::cout << "section text = \"" << section << "\" ";
		}
		else
		{
			std::cout << "getSectionTextBySubscript() failed - " << section << std::endl;
		}
		std::cout << std::endl;
	}
	for (auto& pear : l.section_indexes_by_name)
	{
		std::cout << "Section name \"" << pear.first << "\" has index " << pear.second << std::endl;
	}

	std::list<std::string> listOfString{"[hosts]", "[select]"};

	if (l.containsOnly(my_error_message, listOfString) )
	{
		std::cout << "contains only [hosts], [select]." << std::endl;
	}
	else
	{
		std::cout << "does not contain only [hosts], [select] - " << my_error_message << std::endl;		
	}

	if (l.containsSection(std::string("[HOSTS]")))
	{
		std::cout << "contains [HOSTS]." << std::endl;
	}
	else
	{
		std::cout << "does not contain [HOSTS]." <<  std::endl;		
	}

	if (l.getSectionTextBySectionName(my_error_message,"[HOSTS]",text))
	{
		std::cout << "[HOSTS] text =\"" << text << "\"." << std::endl;
	}
	else
	{
		std::cout << "Retrieval of [HOSTS] text failed - " << my_error_message << std::endl;		
	}

	return;
}


int main(int argc, char* argv[])
{

	IvyscriptLine a("[secion] abnlksajd [sec] ;lkjasdf[secb]");

	describe(a);

	std::cout <<std::endl << "-------------------------------------------------------" << std::endl;

	IvyscriptLine b("[secion4] abnlksajd [hosTS] borkl sun145   [secb]");	
	describe(b);

	std::cout <<std::endl << "-------------------------------------------------------" << std::endl;

	IvyscriptLine c("[secion&&4] abnlksajd [hosTS] borkl sun145   [secb]");
	describe(c);

	std::cout <<std::endl << "-------------------------------------------------------" << std::endl;

	IvyscriptLine d("[hosTS] borkl sun145   [seLECt]");
	describe(d);

	return 0;
}
