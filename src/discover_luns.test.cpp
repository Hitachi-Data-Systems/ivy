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

#include "discover_luns.cpp"

int main (){

	discovered_LUNs luns;
	luns.discover();

	std::string header, lun_line;
	if (!luns.get_header_line(header)) {
		std::cout << "Failed to get header line." << std::endl;
	} else {
		std::cout << "Header=\"" << header << "\"." << std::endl;
		while (luns.get_next_line(lun_line)) {
			std::cout << "LUN=\"" << lun_line << "\"." << std::endl;
		}
	}

	std::cout << "LDEV column=" << luns.get_column_by_header("LDEV") << std::endl;
	std::string value;

	if (luns.get_attribute_value_by_header("LDEV",0,value)) {
	 	std::cout << "LDEV attribute for LDEV #0 is \"" << value << "\"." << std::endl;
	} else {
		std::cout << "Failed to get LDEV attribute for LDEV #0" << std::endl;
	}
	if (luns.get_attribute_value_by_header("LDEV",1,value)) {
	 	std::cout << "LDEV attribute for LDEV #1 is \"" << value << "\"." << std::endl;
	} else {
		std::cout << "Failed to get LDEV attribute for LDEV #1" << std::endl;
	}
	

    return 0;
}

