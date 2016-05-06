//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include "master_stuff.h"
#include "DynamicFeedbackController.h"

std::string DynamicFeedbackController::test_phase(unsigned int subinterval_index)
{
    if (EVALUATE_SUBINTERVAL_SUCCESS == m_s.lastEvaluateSubintervalReturnCode)
    {
        if ( subinterval_index < firstMeasurementSubintervalIndex() )
            return std::string("warmup");
        else if ( subinterval_index <= lastMeasurementSubintervalIndex() )
            return std::string("measurement");
        else
            return std::string("cooldown");
    }
    else
        return std::string("warmup");
}

