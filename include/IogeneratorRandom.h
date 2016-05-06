//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class IogeneratorRandom : public Iogenerator {
	// this is the base class of random_steady and random_independent.
protected:
//        size_t seed_hash;
	std::uniform_int_distribution<uint64_t>* p_uniform_int_distribution {NULL};
	std::uniform_real_distribution<ivy_float>* p_uniform_real_distribution_0_to_1{NULL};
        std::default_random_engine deafrangen;

public:
	IogeneratorRandom(LUN* pL, std::string lf, std::string tK, iogenerator_stuff* p_is) : Iogenerator(pL, lf, tK, p_is) {}

	virtual std::string instanceType()=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.
	bool setFrom_IogeneratorInput(IogeneratorInput*);
	bool generate(Eyeo& slang);
};
