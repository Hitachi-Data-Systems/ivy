//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

