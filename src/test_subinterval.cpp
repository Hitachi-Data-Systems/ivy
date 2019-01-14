//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <iostream>

#include "ivydefines.h"
#include "ivyhelpers.h"
#include "IosequencerInput.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"

int main (int argc, char* argv[])
{
	std::string my_error_message;
	std::string logfile("/home/ivogelesang/ivy/test_subinterval.log.txt");

	IosequencerInput a,b,c,d,e;

	if (!a.setMultipleParameters(my_error_message,std::string("ioGenerator=random_constant_QUEUE, blocksize=7 kib")))
	{
		std::cout << R"raw(a.setMultipleParameters(my_error_message,std::string("ioGenerator=random_constant_QUEUE, blocksize=7 kib")))raw" << " failed - " << my_error_message << std::endl;
	}
	else
	{
		std::cout << R"raw(a.setMultipleParameters(my_error_message,std::string("ioGenerator=random_constant_QUEUE, blocksize=7 kib")))raw" << " successful." << std::endl;
	}

	if (b.fromString(a.toString(), "test_subinterval.logfile.txt"))
	{
		std::cout << "b.fromString(a.toString() was true." << std::endl<< std::endl;
	}
	else
	{
		std::cout << "b.fromString(a.toString() was false." << std::endl<< std::endl;
	}




/*
	if
	(
	)
	{
		std::cout << R"raw()raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw()raw" << " failed - " << my_errorMessage << std::endl;

	}
*/


	if (c.setParameter(my_error_message, "iosequencer = random_inDEPENDENT"))
	{
		std::cout << R"raw(c.setParameter(my_error_message, "iosequencer = random_inDEPENDENT"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(c.setParameter(my_error_message, "iosequencer = random_inDEPENDENT"))raw" << " failed - " << my_error_message << std::endl;

	}

	if (d.setParameter(my_error_message, "iosequencer = seQUENTIAL"))
	{
		std::cout << R"raw(d.setParameter("iosequencer = seQUENTIAL", "test_subinterval.logfile.txt"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(d.setParameter("iosequencer = seQUENTIAL", "test_subinterval.logfile.txt"))raw" << " failed - " << my_error_message << std::endl;

	}


	if (e.setParameter(my_error_message,"iosequencer = random_inDENT"))
	{
		std::cout << R"raw(e.setParameter(my_error_message,"iosequencer = random_inDENT"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(e.setParameter(my_error_message,"iosequencer = random_inDENT"))raw" << " failed - " << my_error_message << std::endl;

	}

	std::cout << "a.toString=\"" << a.toString() << "\"." << std::endl<< std::endl;
	std::cout << "b.toString=\"" << b.toString() << "\"." << std::endl<< std::endl;
	std::cout << "c.toString=\"" << c.toString() << "\"." << std::endl<< std::endl;
	std::cout << "d.toString=\"" << d.toString() << "\"." << std::endl<< std::endl;
	std::cout << "e.toString=\"" << e.toString() << "\"." << std::endl<< std::endl;

	IosequencerInputRollup iir1, iir2;
	std::cout << R"literalstring(iir1.print_values_seen(std::cout);=")literalstring" << std::endl <<std::endl;
	iir1.print_values_seen(std::cout);
	std::cout << "\"." << std::endl;

	if
	(
		iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sda")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sda"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sda"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(
	iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun160","00:ee", "/dev/sdb")
	)
	{
		std::cout << R"raw(	iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun160","00:ee", "/dev/sdb"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(	iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun160","00:ee", "/dev/sdb"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.159","00:ed", "/dev/sda")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.159","00:ed", "/dev/sda"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.159","00:ed", "/dev/sda"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun161","1000", "/dev/sdb")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun161","1000", "/dev/sdb"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun161","1000", "/dev/sdb"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.160","00:ef", "/dev/sdxx")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.160","00:ef", "/dev/sdxx"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,a.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","172.17.19.160","00:ef", "/dev/sdxx"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sdxx")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sdxx"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,b.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","sun159","00:ef", "/dev/sdxx"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb26","00:ef", "/dev/sdxx")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb26","00:ef", "/dev/sdxx"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,c.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb26","00:ef", "/dev/sdxx"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	if
	(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb27","00:ef", "/dev/sdxx")
	)
	{
		std::cout << R"raw(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb27","00:ef", "/dev/sdxx"))raw" << " successful." << std::endl;
	}
	else
	{
		std::cout << R"raw(iir2.add(my_error_message,d.getParameterNameEqualsTextValueCommaSeparatedList(), "threadname1","cb27","00:ef", "/dev/sdxx"))raw" << " failed - " << my_errorMessage << std::endl;

	}

	std::cout << R"literalstring(iir2.print_values_seen(std::cout);=")literalstring" <<std::endl<<std::endl;
	iir2.print_values_seen(std::cout);
	std::cout << "\"." << std::endl;


	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE")) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE")) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE"),true) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE"),true) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE"),false) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("blockSIzE"),false) << std::endl;


	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead")) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead")) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead"),true) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead"),true) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead"),false) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("FractionRead"),false) << std::endl;


	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage")) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage")) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage"),true) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage"),true) << std::endl;

	std::cout << R"literalstring(
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage"),false) << std::endl;
)literalstring";
	std::cout << iir2.getParameterTextValueByName(std::string("seqStartFractionOFCoverage"),false) << std::endl;



	std::cout << R"literalstring(
	std::cout<<iir2.CSVcolumnTitles() << std::endl;
)literalstring";
	std::cout<<iir2.CSVcolumnTitles() << std::endl;

	std::cout << R"literalstring(
	std::cout<<iir2.CSVcolumnValues(true) << std::endl;
)literalstring";
	std::cout<<iir2.CSVcolumnValues(true) << std::endl;

	std::cout << R"literalstring(
	std::cout<<iir2.CSVcolumnValues(false) << std::endl;
)literalstring";
	std::cout<<iir2.CSVcolumnValues(false) << std::endl;



	std::cout << R"literalstring(
	std::cout << a.getParameterNameEqualsTextValueCommaSeparatedList() << std::endl;
)literalstring";
	std::cout << a.getParameterNameEqualsTextValueCommaSeparatedList() << std::endl;

	std::string createThreadName="bork36";
	std::string createThreadHost="sun159";
	std::string createThreadLDEV="00FF";
	std::string createThreadLUN="/dev/sdxx";
	std::string createThreadKey = createThreadName+std::string(":")+createThreadHost
				+std::string(":")+createThreadLDEV+std::string(":")+createThreadLUN;
	SubintervalOutput sub_out;
	for (int i=0; i<SubintervalOutput::RunningStatCount(); i++)
	{
		for (ivy_float ld=1000.0; ld <10001.; ld+=1000.)
		{
			sub_out.u.accumulator_array[i].Push(ld);
		}
	}

	{
		// make a line just like we get from ivydriver
		ostringstream o;
		o << "<" << createThreadKey << ">"
			<< a.toString()
			<< sub_out.toString();
		std::cout << o.str() << std::endl;

		std::cout << std::endl << "line was " << o.str().length() << " bytes long" << std::endl;
	}

	return 0;
}
