//Copyright (c) 2016-2020 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <iostream>

#include "ivytime.h"

int main(int argc, char* argv[])
{
    std::cout << "Hello, whirrled!" << std::endl;



    ivytime       one_point_five (1.5);
    ivytime minus_one_point_five (-1.5);

    std::cout << "one_point_five:" << one_point_five.toString()
        << " duration \"" << one_point_five.format_as_duration_HMMSSns() << "\"." << std::endl;

    std::cout << "minus_one_point_five:" << minus_one_point_five.toString()
        << " \"" << minus_one_point_five.format_as_duration_HMMSSns() << "\"." << std::endl;



    ivytime point_one (0.1);
    ivytime minus_point_one (-0.1);

    std::cout << "point_one:" << point_one.toString()
        << " duration \"" << point_one.format_as_duration_HMMSSns() << "\"." << std::endl;

    std::cout << "minus_point_one:" << minus_point_one.toString()
        << " \"" << minus_point_one.format_as_duration_HMMSSns() << "\"." << std::endl;







    ivytime point_five (0.5);
    ivytime minus_point_five(-.5);

    std::cout << "point_five:" << point_five.toString()
        << " duration \"" << point_five.format_as_duration_HMMSSns() << "\"." << std::endl;

    std::cout << "minus_point_five:" << minus_point_five.toString()
        << " \"" << minus_point_five.format_as_duration_HMMSSns() << "\"." << std::endl;


    std::cout << std::endl;

    ivytime bork (-1.5);

    for (unsigned int i = 0; i < 20; i++)
    {
        std::cout << "before " << bork.format_as_duration_HMMSSns() << " or " << bork.format_as_duration_HMMSS();
        bork = bork + point_one;
        std::cout << ", after " << bork.format_as_duration_HMMSSns() << " or " << bork.format_as_duration_HMMSS() << std::endl;
    }

    std::cout << std::endl;

    bork = ivytime(1.5);

    for (unsigned int i = 0; i < 20; i++)
    {
        std::cout << "before " << bork.format_as_duration_HMMSSns() << " or " << bork.format_as_duration_HMMSS();
        bork = bork - point_one;
        std::cout << ", after " << bork.format_as_duration_HMMSSns() << " or " << bork.format_as_duration_HMMSS() << std::endl;
    }


    return 0;
}


