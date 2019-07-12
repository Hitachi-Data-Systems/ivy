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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include <climits>  // for UINT_MAX
#include <string>
#include <stdexcept>

#include "ivydefines.h"

//When a MeasureController sends out an update,
//
//	- It provides the name of rollup type (AttributeNameCombo) to send to, like "serial_number+port"
//
//	- It provides a reference to a std::list<std::string> containing one or more AttributeValueCombo instances, like 410123+1A or {410123+1A 410123+3A}
//
//		- The "all" rollup has one instance called "all".
//
//			- If you want an "each" rollup, you can get this by specifying "[rollup] host LUN_name workload"
//
//	- It specifies parameter settings just like you set in [CreateWorkload] like "IOPS=356, fractionRead=.25"
//
//		- Note that not all iosequencer settings are referred to after the first sibinterval.
//		  Go ahead and say "maxTags = 32, or "blocksize = 8 KiB" or "RangeEnd = .8";
//		  there will be no error message and the updates will be delivered into the next subinterval's input section
//		  before the subinterval starts, but if the I/O sequencer being used doesn't refer to the
//		  parameter after starting, changing the parameter has no effect.
//
//		- maxTags is a special case, because when an I/O generator start running, it creates a Linux
//		  AIO "context" with the number of I/O tracking slots set to the value of the maxTags parameter.
//		  So we could easily enough change the maxTags value smaller than that once it's running by only using part of the AIO context,
//		  but we can't make it bigger without quiescing all I/O, and destroying the AIO context and building a bigger one.
//
//		TO DO:  For each iosequencer type, make a list of those parameters that are only referred to at initial startup and that are not referred to after that.
//

enum class DFCcategory { measure };  // There used to be fixed, warble_Sun159, PID, IOPS_at_WP DFCs, but now there is only measure.

#define EVALUATE_SUBINTERVAL_FAILURE -1
#define EVALUATE_SUBINTERVAL_CONTINUE 0
#define EVALUATE_SUBINTERVAL_SUCCESS 1
// would have used enum class today.

class MeasureController
{
public:
//variables

//methods
	MeasureController(){};

	virtual std::string name() = 0;

	virtual int evaluateSubinterval() = 0;

	virtual void step_over_cooldown_subinterval() = 0;

	virtual void reset()=0;

	virtual DFCcategory category()=0;

    virtual ~MeasureController();
};


