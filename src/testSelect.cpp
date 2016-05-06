//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
//testSelect.cpp

#include <iostream>

#include "LUN.h"
#include "Select.h"
#include "SelectClause.h"
#include "LUNpointerList.h"
#include "LDEVset.h"
#include "Matcher.h"

int main(int argc, char* argv[])
{
	std::string logfilename="testSelect.log.txt";

	std::string headerline("ivyscript_hostname"  ",slash dev name"  ",port"  ",LDEV"   ",LDEV Type"  ",CLPR"   ",Parity Group" ",DP Pool ID");
	std::string dataline1 ("bork0"               ",/dev/sd02"       ",1A"    ",00:00"  ",Internal"   ",CLPR0"  ",1-1"          ",");
	std::string dataline2 ("bork1"               ",/dev/sd03"       ",3A"    ",00:01"  ",Internal"   ",CLPR1"  ",1-2"          ",");
	std::string dataline3 ("bork2"               ",/dev/sd04"       ",5A"    ",00:02"  ",DP-Vol"     ",CLPR1"  ","             ",1");
	std::string dataline4 ("torque"              ",/dev/sd05"       ",7A"    ",00:03"  ",DP-Vol"     ",CLP01"  ","             ",2");

	std::string dataline10 ("bork0"               ",/dev/sd10"       ",1A"    ",01:01"  ",Internal"   ",CLPR0"  ",1-1"          ",");
	std::string dataline11 ("bork0"               ",/dev/sd11"       ",1A"    ",01:02"  ",Internal"   ",CLPR0"  ",1-2"          ",");
	std::string dataline12 ("bork0"               ",/dev/sd12"       ",1A"    ",01:03"  ",Internal"   ",CLPR0"  ",01-03"        ",");
	std::string dataline13 ("bork0"               ",/dev/sd13"       ",1A"    ",01:04"  ",Internal"   ",CLPR0"  ",01-04"        ",");
	std::string dataline14 ("bork0"               ",/dev/sd14"       ",1A"    ",02:01"  ",Internal"   ",CLPR0"  ",02-1"         ",");
	std::string dataline15 ("bork0"               ",/dev/sd15"       ",1A"    ",02:02"  ",Internal"   ",CLPR0"  ",2-2"          ",");
	std::string dataline16 ("bork0"               ",/dev/sd16"       ",1A"    ",02:03"  ",Internal"   ",CLPR0"  ",02-03"        ",");

	LUNpointerList psLUN;
	LUN* p_LUN1 = new LUN; p_LUN1->loadcsvline(headerline,dataline1, logfilename);
	LUN* p_LUN2 = new LUN; p_LUN2->loadcsvline(headerline,dataline2, logfilename);
	LUN* p_LUN3 = new LUN; p_LUN3->loadcsvline(headerline,dataline3, logfilename);
	LUN* p_LUN4 = new LUN; p_LUN4->loadcsvline(headerline,dataline4, logfilename);

	std::cout << "p_LUN1->toString() = \"" << p_LUN1->toString() << "\"." << std::endl;
	std::cout << "p_LUN2->toString() = \"" << p_LUN2->toString() << "\"." << std::endl;
	std::cout << "p_LUN3->toString() = \"" << p_LUN3->toString() << "\"." << std::endl;
	std::cout << "p_LUN4->toString() = \"" << p_LUN4->toString() << "\"." << std::endl;

	psLUN.LUNpointers.push_back(p_LUN1);
	psLUN.LUNpointers.push_back(p_LUN2);
	psLUN.LUNpointers.push_back(p_LUN3);
	psLUN.LUNpointers.push_back(p_LUN4);

	LUN* pdl10 = new LUN; pdl10->loadcsvline(headerline,dataline10,logfilename); psLUN.LUNpointers.push_back(pdl10);
	LUN* pdl11 = new LUN; pdl11->loadcsvline(headerline,dataline11,logfilename); psLUN.LUNpointers.push_back(pdl11);
	LUN* pdl12 = new LUN; pdl12->loadcsvline(headerline,dataline12,logfilename); psLUN.LUNpointers.push_back(pdl12);
	LUN* pdl13 = new LUN; pdl13->loadcsvline(headerline,dataline13,logfilename); psLUN.LUNpointers.push_back(pdl13);
	LUN* pdl14 = new LUN; pdl14->loadcsvline(headerline,dataline14,logfilename); psLUN.LUNpointers.push_back(pdl14);
	LUN* pdl15 = new LUN; pdl15->loadcsvline(headerline,dataline15,logfilename); psLUN.LUNpointers.push_back(pdl15);
	LUN* pdl16 = new LUN; pdl16->loadcsvline(headerline,dataline16,logfilename); psLUN.LUNpointers.push_back(pdl16);

	LUN* TheSampleLUN = p_LUN1;

	Select* p_Select1 = new Select ("",TheSampleLUN,"testSelect.log.txt"); 
	std::cout << p_Select1->toString() << std::endl;

	Select* p_Select2 = new Select (" port = 1a ",TheSampleLUN,"testSelect.log.txt"); 
	std::cout << p_Select2->toString() << std::endl;

	Select* p_Select3 = new Select (" port = { 1a , 3A } ",TheSampleLUN,"testSelect.log.txt"); 
	std::cout << p_Select3->toString() << std::endl;

	Select* p_Select4 = new Select ("port=1a ,CLPR =  CLPR0  ",TheSampleLUN,"testSelect.log.txt"); 
	std::cout << p_Select4->toString() << std::endl;

	Select* p_Select5 = new Select ("port = {1a, 3A},CLPR = { CLIPPERSHIP, CLPR0 BORK }",TheSampleLUN,"testSelect.log.txt"); 
	std::cout << p_Select5->toString() << std::endl;

	std::cout << std::endl;

	std::cout << "psLUN contains:" << std::endl << psLUN.toString() << std::endl;
	
	LUNpointerList filteredBySelect1,filteredBySelect2,filteredBySelect3,filteredBySelect4,filteredBySelect5;
	
	filteredBySelect1.clear_and_set_filtered_version_of(psLUN, p_Select1, logfilename);
	std::cout << "filteredBySelect1 contains:" << std::endl << filteredBySelect1.toString() << std::endl;

	filteredBySelect2.clear_and_set_filtered_version_of(psLUN, p_Select2, logfilename);
	std::cout << "filteredBySelect2 contains:" << std::endl << filteredBySelect2.toString() << std::endl;

	filteredBySelect3.clear_and_set_filtered_version_of(psLUN, p_Select3, logfilename);
	std::cout << "filteredBySelect3 contains:" << std::endl << filteredBySelect3.toString() << std::endl;

	filteredBySelect4.clear_and_set_filtered_version_of(psLUN, p_Select4, logfilename);
	std::cout << "filteredBySelect4 contains:" << std::endl << filteredBySelect4.toString() << std::endl;

	filteredBySelect5.clear_and_set_filtered_version_of(psLUN, p_Select5, logfilename);
	std::cout << "filteredBySelect5 contains:" << std::endl << filteredBySelect5.toString() << std::endl;

	std::cout << "before making new Select." << std::endl;

	Select* pS = new Select("LDEV = {00:01, 00:02, 00:03}",TheSampleLUN,"testSelect.log.txt"); 

	std::cout << "before making filteredOnLDEVs" << std::endl;

	LUNpointerList filteredOnLDEVs;

	std::cout << "made filteredOnLDEVs" << std::endl;

	filteredOnLDEVs.clear_and_set_filtered_version_of(psLUN, pS, logfilename);
	std::cout << "filteredOnLDEVs contains:" << std::endl << filteredOnLDEVs.toString() << std::endl;


	Select* pShn = new Select("host = {bork1-2, torque}",TheSampleLUN,"testSelect.log.txt"); 

	LUNpointerList filteredOnHost;

	filteredOnHost.clear_and_set_filtered_version_of(psLUN, pShn, logfilename);
	std::cout << "filteredOnHost contains:" << std::endl << filteredOnHost.toString() << std::endl;

	Select* pPGSelect = new Select("pg = {002-*, 1-2:4}",TheSampleLUN,"testSelect.log.txt"); 

	std::cout << pPGSelect->toString() << std::endl;

	std::cout << "before making filteredOnPGs" << std::endl;


	LUNpointerList filteredOnPGs;

	std::cout << "made filteredOnPGs" << std::endl;

	filteredOnPGs.clear_and_set_filtered_version_of(psLUN, pPGSelect, logfilename);

	std::cout << "filteredOnPGs contains:" << std::endl << filteredOnPGs.toString() << std::endl;

	return 0;
}
