//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class SequenceOfSubintervalRollup
{
	// This is the thing that the DynamicFeedbackController primarily looks at.

	// You get one of these for each rollup instance

	// For the host rollup
	//     sun159      instance has a SubintervalRollupSequence
	//     192.168.1.1 instance has a SubintervalRollupSequence
	

public:
//variables	
	std::vector<SubintervalRollup> sequence;
	void addSubinterval() { SubintervalRollup sR; sequence.push_back(sR); }
	SubintervalRollup& lastSubintervalRollup() {return sequence[sequence.size()-1];}
};


