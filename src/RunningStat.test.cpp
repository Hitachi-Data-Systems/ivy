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
#include "RunningStat.h"

int main(int argc, char* argv[]) {

	RunningStat rs;
	rs.Push(3.1415);
	rs.Push(.1415);
	rs.Push(10);
	
	std::cout << "rs=" << rs.toString() << std::endl;

	rs.Clear();
	std::cout << "rs=" << rs.toString() << std::endl;

	//if (rs.fromString(std::string("<3;4.42767;51.0763;92.1562;1304.4;0.1415;10>")))
	if (rs.fromString(std::string("<3;4.42767;51.0763;0.1415;10>")))
		std::cout << "fromString() success" << std::endl;
	else
		std::cout << "fromString() failure" << std::endl;
	std::cout << "rs=" << rs.toString() << std::endl;

	return 0;
}
