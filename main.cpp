//
//  main.cpp
//

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <functional>
#include <string>
#include <map>
#include "Testing.hpp"

std::string getLocalFolder() {
    //return std::string("c:/xxx/yyy");
    return std::string("/tmp"); //SET PATH TO LOCAL ARCHIVE FOLDER
}

int main(int argc, const char * argv[]) {
    srand(time(NULL));
    if(argc>1) {
        std::string temp(argv[1]);
        std::stringstream theOutput;

        std::string theFolder(getLocalFolder());
        if(3==argc) theFolder=argv[2];
        ECE141::Testing theTester(theFolder);

        using TestCall = std::function<bool()>;
        static std::map<std::string, TestCall> theCalls {
                {"Compile", [&](){return true;}  },
                {"Create",  [&](){return theTester.doCreateTests(theOutput);}  },
                {"Open",    [&](){return theTester.doOpenTests(theOutput);}  },
                {"Add",     [&](){return theTester.doAddTests(theOutput);}  },
                {"Extract", [&](){return theTester.doExtractTests(theOutput);}  },
                {"Remove",  [&](){return theTester.doRemoveTests(theOutput);}  },
                {"List",    [&](){return theTester.doListTests(theOutput);}  },
                {"Dump",    [&](){return theTester.doDumpTests(theOutput);}  },
                {"Stress",  [&](){return theTester.doStressTests(theOutput);}  },
                {"Compress",  [&](){return theTester.doCompressTests(theOutput);}  },
                {"All",     [&](){return theTester.doAllTests(theOutput);}  },
        };

        std::string theCmd(argv[1]);
        if(theCalls.count(theCmd)) {
            bool theResult = theCalls[theCmd]();
            const char* theStatus[]={"FAIL","PASS"};
            std::cout << theCmd << " test " << theStatus[theResult] << "\n";
            std::cout << "------------------------------\n"
                      << theOutput.str() << "\n";
        }
        else std::cout << "Unknown test\n";

    }

    return 0;
}
