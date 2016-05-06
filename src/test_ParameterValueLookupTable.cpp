//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
