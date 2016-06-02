#include "ivyhelpers.h"
#include "pattern.h"

std::string valid_patterns() {return ("Valid pattern values are \"random\", \"gobbledegook\", \"ascii\", and \"trailing_zeros\".");}

pattern parse_pattern(std::string s)
{
    trim(s);
    if (stringCaseInsensitiveEquality(s,std::string("random")))         { return pattern::random; }

    if (stringCaseInsensitiveEquality(s,std::string("gobbledegook")))   { return pattern::gobbledegook;}

    if (stringCaseInsensitiveEquality(s,std::string("ascii")))          { return pattern::ascii; }

    if (stringCaseInsensitiveEquality(s,std::string("trailing_zeros"))) { return pattern::trailing_zeros; }

    return pattern::invalid;
}

std::string pattern_to_string(pattern p)
{
    switch (p)
    {
        case pattern::ascii: return "ascii";
        case pattern::gobbledegook: return "gobbledegook";
        case pattern::random: return "random";
        case pattern::trailing_zeros: return "trailing_zeros";
        default: return "invalid_pattern";
    }
    return "invalid_pattern";
}
