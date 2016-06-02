#pragma once

enum class pattern {invalid = 0, random, gobbledegook, ascii, trailing_zeros};

std::string valid_patterns();

pattern parse_pattern(std::string);

std::string pattern_to_string(pattern);
