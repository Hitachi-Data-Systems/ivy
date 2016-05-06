//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
