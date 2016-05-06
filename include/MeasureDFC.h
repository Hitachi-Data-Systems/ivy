//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "DynamicFeedbackController.h"

class MeasureDFC : public DynamicFeedbackController
{
public:
// variables
	unsigned int measureFirstIndex {UINT_MAX}, measureLastIndex {UINT_MAX};

	int current {-1};

//methods
	MeasureDFC() : DynamicFeedbackController() {}

	std::string name() override {return std::string("Measure");}

	int evaluateSubinterval() override;

	unsigned int firstMeasurementSubintervalIndex() override { return measureFirstIndex; }

	unsigned int lastMeasurementSubintervalIndex() override { return measureLastIndex; }

	void reset() override;

	virtual DFCcategory category() override {return DFCcategory::measure;}



};


