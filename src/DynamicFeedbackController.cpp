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
#include "ivy_engine.h"
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

