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

#include "ivydefines.h"
#include "LeastSquaresLine.h"

int main (int argc, char* argv[])
{

	LeastSquaresLine<double> l(3);

	l.toOstream(std::cout);	
	l.push(1.,2.); 	std::cout << "l.push(1.,2.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;	
	l.push(2.,4.); 	std::cout << "l.push(2.,4.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;	
	l.push(3.,6.); 	std::cout << "l.push(3.,6.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;
	l.push(5.,6.); 	std::cout << "l.push(5.,6.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;
	l.push(7.,8.); 	std::cout << "l.push(7.,8.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;
	l.push(9.,10.);	std::cout << "l.push(9.,10.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;	
	l.push(1.,1.); 	std::cout << "l.push(1.,1.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;	
	l.push(2.,3.); 	std::cout << "l.push(2.,3.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;	
	l.push(3.,5.); 	std::cout << "l.push(3.,5.);" << std::endl; l.toOstream(std::cout); std::cout << std::endl;
	
}

