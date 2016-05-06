//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class IogeneratorRandomSteady : public IogeneratorRandom {
public:
	IogeneratorRandomSteady(LUN* pL, std::string logfilename, std::string tK, iogenerator_stuff* p_is) : IogeneratorRandom(pL, logfilename, tK, p_is) {}

	std::string instanceType() { return std::string("random_steady"); }
	bool isRandom() { return true; }
	bool generate(Eyeo&);
};

