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

#include "LUN.h"

int main(int argc, char* argv[])
{

	std::string logfilename("/home/ivogelesang/ivy/testLUN.log.txt");

	std::string headerline="hostname,SCSI Bus Number (HBA),LUN Name,Hitachi Product,HDS Product,Serial Number,Port,LUN 0-255,LDEV,Nickname,LDEV type,RAID level,Parity Group,Pool ID,CLPR,Max LBA,Size MB,Size MiB,Size GB,Size GiB,SizeTB,Size TiB,Vendor,Product,Product Revision,SCSI_IOCTL_PROBE_HOST";

	std::string datalineRAID = "cb23,2,/dev/sdbu,HM700,HUS VM,210030,1A,0,00:00,,Internal,RAID-1,01-01,,CLPR0,2097151,1073.741824,1024.000000,1.073742,1.000000,0.001074,0.000977,HITACHI ,OPEN-V          ,7303,EMC LPe12002-E 8Gb 2-port PCIe Fibre Channel Adapter on PCI bus 2e device 00 irq 16 port 0 Logical L";

	std::string datalineDF = "172.17.19.159,1,/dev/sdb,DF800S,AMS2100,83011441,2A,4,0004,,,,,,,2247098367,1150514.364416,1097216.000000,1150.514364,1071.500000,1.150514,1.046387,HITACHI ,DF600F          ,0000,Emulex LPe11002-S 4Gb 2-port PCIe FC HBA on PCI bus 21 device 00 irq 59 port 0 Logical Link Speed: 4";

	LUN RAIDLUN, DFLUN;
	RAIDLUN.loadcsvline(headerline,datalineRAID,logfilename);
	DFLUN.loadcsvline(headerline,datalineDF,logfilename);

	std::cout << "RAIDLUN =" << RAIDLUN.toString() << std::endl <<std::endl;
	
	std::cout << "DFLUN =" << DFLUN.toString() << std::endl <<std::endl;

	if (RAIDLUN.contains_attribute_name(" PARITY GROUP "))
		std::cout << "contains \" PARITY GROUP \"" << std::endl; 
	else
		std::cout << "does not contain \" PARITY GROUP \"" << std::endl; 

	if (RAIDLUN.contains_attribute_name(" chARITY GROUP "))
		std::cout << "contains \" chARITY GROUP \"" << std::endl; 
	else
		std::cout << "does not contain \" chARITY GROUP \""<<std::endl; 


	if (RAIDLUN.attribute_value_matches("\"PORT\"", "\"1a\""))
		std::cout << "True: RAIDLUN.attribute_value_matches(\"PORT\", \"1a\")" << std::endl << std::endl;
	else
		std::cout << "False: RAIDLUN.attribute_value_matches(\"PORT\", \"1a\")" << std::endl << std::endl;


	if (RAIDLUN.attribute_value_matches("\"PORT\"", "\"1A\""))
		std::cout << "True: RAIDLUN.attribute_value_matches(\"PORT\", \"1A\")" << std::endl << std::endl;
	else
		std::cout << "False: RAIDLUN.attribute_value_matches(\"PORT\", \"1A\")" << std::endl << std::endl;


	if (RAIDLUN.attribute_value_matches("\"PORT\"", "\"2A\""))
		std::cout << "True: RAIDLUN.attribute_value_matches(\"PORT\", \"2A\")" << std::endl << std::endl;
	else
		std::cout << "False: RAIDLUN.attribute_value_matches(\"PORT\", \"2A\")" << std::endl << std::endl;

	std::cout << "RAIDLUN.attribute_value(\"parity_group\") = \"" << RAIDLUN.attribute_value("parity_group") << "\"." << std::endl;

	return 0;
}
