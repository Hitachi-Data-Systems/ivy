//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#pragma once

#include "IogeneratorInputRollup.h"

class SubintervalRollup
{
public:
// variables
	IogeneratorInputRollup inputRollup;
	SubintervalOutput outputRollup;
	ivytime startIvytime, endIvytime;
//methods
	SubintervalRollup(ivytime subinterval_start, ivytime subinterval_end) : startIvytime(subinterval_start), endIvytime(subinterval_end) {}
	SubintervalRollup() {startIvytime = ivytime(0); endIvytime = ivytime(0);}
	void clear() {inputRollup.clear(); outputRollup.clear(); startIvytime=ivytime(0); endIvytime=ivytime(0);}
	void addIn(const SubintervalRollup& other);
	ivy_float durationSeconds() { ivytime dur = endIvytime - startIvytime; return dur.getlongdoubleseconds();}
};


