
#include "ivyhelpers.h"

using namespace std;
//
//
//std::map<unsigned int, std::vector<unsigned int>> get_processors_by_core()
//{
////    # cat /proc/cpuinfo | grep 'processor\|core id'
////    processor	: 0
////    core id		: 0
////    processor	: 1
////    core id		: 1
////    processor	: 2
////    core id		: 2
////    processor	: 3
////    core id		: 3
////    processor	: 4
////    core id		: 0
////    processor	: 5
////    core id		: 1
////    processor	: 6
////    core id		: 2
////    processor	: 7
////    core id		: 3
////    processor	: 8
////    core id		: 0
////    processor	: 9
////    core id		: 1
////    processor	: 10
////    core id		: 2
////    processor	: 11
////    core id		: 3
////    processor	: 12
////    core id		: 0
////    processor	: 13
////    core id		: 1
////    processor	: 14
////    core id		: 2
////    processor	: 15
////    core id		: 3
//
//    std::string command {  R"(cat /proc/cpuinfo | grep 'processor\|core id')"  };
//
//    std::string command_output = GetStdoutFromCommand(command);
//
//    std::istringstream iss(command_output);
//
//    std::string line;
//
//    unsigned int processor {0}, core_id {0};
//
//    std::regex processor_regex(   R"(processor[ \t]*:[ \t]*([0-9]+))"  );
//    std::regex core_id_regex  (   R"(core id[ \t]*:[ \t]*([0-9]+))"    );
//
//    std::smatch     smash;
//    std::ssub_match subm;
//
//    std::map<unsigned int, std::vector<unsigned int>> processors_by_core {};
//
//    while (std::getline(iss, line))
//    {
//        if (std::regex_match(line, smash, processor_regex))
//        {
//            subm = smash[1];
//            std::string number = subm.str();
//            {
//                std::istringstream iss_num(number);
//                iss_num >> processor;
//            }
//        }
//        else if (std::regex_match(line, smash, core_id_regex))
//        {
//            subm = smash[1];
//            std::string number = subm.str();
//            {
//                std::istringstream iss_num(number);
//                iss_num >> core_id;
//            }
//            processors_by_core[core_id].push_back(processor);
//        }
//    }
//    return processors_by_core;
//}
//

int main()
{
    for (auto& v : get_processors_by_core())
    {
        std::cout << "core " << v.first << " has hyperthreads ";

        for (auto& p : v.second)
        {
            std::cout << p << "  ";
        }
        std::cout << std::endl;
    }

    return 0;
}
