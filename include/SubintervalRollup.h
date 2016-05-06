//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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


