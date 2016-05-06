//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
using namespace std;

#include <string>
#include <random>
#include <fcntl.h>  // for open64, O_RDWR etc.
#include <functional> // for std::hash
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <list>
#include <map>
#include <algorithm> // for ivyhelpers.h
#include <linux/aio_abi.h>
#include <iostream>
#include <sstream>

#include "ivyhelpers.h"
#include "ivytime.h"
#include "ivydefines.h"
#include "iogenerator_stuff.h"
#include "IogeneratorInput.h"
#include "LUN.h"
#include "Eyeo.h"
#include "WorkloadID.h"
#include "Iogenerator.h"
#include "IogeneratorRandom.h"
#include "IogeneratorRandomSteady.h"



bool IogeneratorRandomSteady::generate(Eyeo& slang)
{
	if (!IogeneratorRandom::generate(slang))
		return false;

	if (-1 == p_IogeneratorInput->IOPS)
	{	// iorate=max
		slang.scheduled_time = ivytime(0);
	}
	else
	{
		if (ivytime(0) == previous_scheduled_time)
		{
			slang.scheduled_time.setToNow();
		}
		else
		{
			slang.scheduled_time = previous_scheduled_time + ivytime(1/(p_IogeneratorInput->IOPS));
		}
		previous_scheduled_time = slang.scheduled_time;
	}
//*debug*/ { ostringstream o; o << "IogeneratorRandomSteady::generate() - IOPS = " << p_IogeneratorInput->IOPS << ", scheduled_time = " << slang.scheduled_time.format_as_datetime_with_ns() << std::endl; log(logfilename,o.str());}
	return true;
}

