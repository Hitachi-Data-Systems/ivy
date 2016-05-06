//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

