#pragma once

#include <string>

#include "ivydefines.h"

bool lookupDoubleSidedStudentsTMultiplier(
    std::string&         // callers_error_message
    , ivy_float&         // return_value
    , const int          // sample_count
    , const std::string& // confidence
);

