//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include "LDEVset.cpp"

void tryadding(LDEVset* set, std::string s) {
	if (!(*set).add(s)) std::cout << "added \"" << s << "\"." << std::endl;
	else std::cout << "failed on \"" << s << "\"." << std::endl;
}

int main (int argc, char* argv[]) {

	LDEVset set;
	tryadding(&set, "0010");
	tryadding(&set, "00:14");
	tryadding(&set, "0010");
	tryadding(&set, "0012");
	tryadding(&set, "Ff41");
	tryadding(&set, "FF:42");
	tryadding(&set, "0011");

	tryadding(&set, "0200-02ff 03:00-03ff 1234 45:23");

	std::cout << "Range " << set.toString() << std::endl;

}
