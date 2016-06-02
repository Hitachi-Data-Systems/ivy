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


#include <string>
#include <iostream>
#include <fstream>
#include <set>
#include <ctype.h> // for isapha()
#include <chrono>

#include "ivydefines.h"

// reads an input text file (Webster's 1913 public source dictionary http://www.gutenberg.org/ebooks/29765)

// writes out a c++ source file for an array of unique words

//


int main(int argc, char* argv[])
{
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    std::set<std::string> unique_words;

    if (argc !=2)
    {
        std::cout << argv[0] << " takes one command line parameter - the name of a text file to be scanned for unique alphabetic words." << std::endl;
        return -1;
    }

    std::ifstream input_file (argv[1]);
    if (!input_file.is_open() || input_file.fail())
    {
        std::cout << "Input file \"" << argv[1] << "\" failed to open." << std::endl;
        return -1;
    }

    std::string word;
    char c;

    while (true)
    {

        if (unique_words.size() >= unique_word_count) break; // this makes a .o of about 5 MB - this is enough words to make every block written unique

        word.clear();

        while (input_file.get(c) && !isalpha(c)) {}

        if (input_file.eof() || input_file.bad())
        {
            if (word.size()>0) unique_words.insert(word);
            break;
        }

        word.push_back(c);

        while (input_file.get(c) && isalpha(c)) {word.push_back(c);}

        unique_words.insert(word);  // case sensitive

        if (input_file.eof() || input_file.bad())
        {
            break;
        }
    }


    std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();

    std::cout << "Building set took "
              << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count()
              << "us.\n";

    std::cout << "There were " << unique_words.size() << " words." << std::endl;

    if (unique_words.size()>0)
    {
        std::ofstream oaf("src/unique_words.cpp");

        oaf
            << "\t" << "const char* unique_words[] =" << std::endl
            << "\t" << "{" << std::endl << "\t\t";

        unsigned int i = 0;
        for (auto it = unique_words.begin(); it != unique_words.end(); it++)
        {
            i++;
            oaf << "\"" << *it << "\"";

            if (i < unique_words.size())
            {
                oaf << ", ";
                if (0 == (i % 10)) {oaf << std::endl << "\t\t";}
            }
        }


        oaf << std::endl << "\t" << "};" << std::endl;

        oaf.close();
    }


}
