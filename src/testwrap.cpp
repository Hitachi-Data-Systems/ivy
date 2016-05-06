//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <list>

#include "ivyhelpers.h"

int main (int arc, char* argv[])
{

    std::list<std::string> input
    {
        "", ",", "a,", ",a", "a", "1", "1.3E-5", "1.5%",
        "\"a\"", "\"a", "a\"",
        ",,,"
        " , , , "
        "a,bee,sea,5,  56,1.45E-03,henrey,2,",
        "\",\",",
        "eh,bee,cee",
        "eh,Be,see2,defg"
    };

    for (auto& s : input)
    {
        std::cout
            << "for input string   \"" << s << "\" - " << countCSVlineUnquotedCommas(s) << std::endl
            << "wrapped up you get \"" << quote_wrap_csvline_except_numbers(s) << "\" - " << countCSVlineUnquotedCommas(quote_wrap_csvline_except_numbers(s))  << std::endl << std::endl;
    }
}
