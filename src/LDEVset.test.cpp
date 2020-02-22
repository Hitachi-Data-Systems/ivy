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
#include "logger.h"
#include "LDEVset.h"
#include "LDEVset.cpp"

void tryadding(LDEVset* set, std::string s) {
	logger logfile {""};
	if (!(*set).add(s,logfile)) std::cout << "added \"" << s << "\"." << std::endl;
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
