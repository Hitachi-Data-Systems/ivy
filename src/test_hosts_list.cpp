#include <vector>
#include "hosts_list.h"

int main(int argc, char* argv[])
{
    std::vector<std::string> names
    {
        "xxx",
        "xxx[0-4]",
        "xxx, yy[1-4], yyy[01-3]",
        "zz[09-12]",
        "a-b",
        "a-b[0-1]",
        "a-b[0-0]",
        "fh%a",
        "",

    };

    hosts_list hl;

    for (auto& s : names)
    {
        hl.clear();
        if (!hl.compile(s))
        {
            std::cout << "<Error> failed parsing list of test hosts \"" << s << "\" - " << hl.message;
        }
        else
        {
            std::cout << hl << std::endl << std::endl << std::endl;
        }
    }

    return 0;
}
