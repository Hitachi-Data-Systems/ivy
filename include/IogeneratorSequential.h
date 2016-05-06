//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class IogeneratorSequential : public Iogenerator {
public:
	IogeneratorSequential(LUN* pL, std::string lf, std::string tK, iogenerator_stuff* p_is) : Iogenerator(pL, lf, tK, p_is) {}

	bool generate(Eyeo&);
	bool setFrom_IogeneratorInput(IogeneratorInput*); 
	bool isRandom() { return false; }
	std::string instanceType() { return std::string("sequential"); }
	long long int lastIOblockNumber=0;  // default of zero means block 1 will be the first one read or written.
};


