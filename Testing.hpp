//
//  Testing.hpp
//
//  Created by rick gessner on 1/24/23.
//

#ifndef Testing_h
#define Testing_h

#include "Archive.hpp"
#include "Tracker.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <filesystem>
#include <cstring>
#include <chrono>

//If you are having trouble with this line make sure you are using C++17
namespace fs = std::filesystem;

namespace ECE141 {

    const size_t kMinBlockSize{1024};

    struct Testing : public ArchiveObserver {

        //-------------------------------------------
        std::string folder;
        std::vector<std::string> stressList;

        std::string getRandomWord() {
            static std::vector<std::string> theWords = {
                    std::string("class"),   std::string("happy"),
                    std::string("coding"),  std::string("pattern"),
                    std::string("design"),  std::string("method"),
                    std::string("dyad"),    std::string("story"),
                    std::string("monad"),   std::string("data"),
                    std::string("compile"), std::string("debug"),
            };
            return theWords[rand() % theWords.size()];
        }

        void makeFile(const std::string& aFullPath, size_t aMaxSize) {
            const char* thePrefix = "";
            std::ofstream theFile(aFullPath.c_str(), std::ios::trunc | std::ios::out);
            size_t theSize = 0;
            size_t theCount = 0;
            while (theSize < aMaxSize) {
                std::string theWord = getRandomWord();
                theFile << thePrefix;
                if (0 == theCount++ % 10) theFile << "\n";
                theFile << theWord;
                thePrefix = ", ";
                theSize += theWord.size() + 2;
            }
            theFile << std::endl;
            theFile.close();
        }

        void buildTestFiles() {
            makeFile(folder + "/smallA.txt", 890);
            makeFile(folder + "/smallB.txt", 890);
            makeFile(folder + "/mediumA.txt", 1780);
            makeFile(folder + "/mediumB.txt", 1780);
            makeFile(folder + "/largeA.txt", 2640);
            makeFile(folder + "/largeB.txt", 2640);
        }

        Testing(const std::string& aFolder) : folder(aFolder) {
            buildTestFiles();
        }

        bool doAllTests(std::ostream& anOutput) {
            return doCreateTests(anOutput) &&
                   doOpenTests(anOutput) &&
                   doAddTests(anOutput) &&
                   doExtractTests(anOutput) &&
                   doRemoveTests(anOutput) &&
                   doListTests(anOutput) &&
                   doDumpTests(anOutput) &&
                   doStressTests(anOutput);
        }

        //-------------------------------------------

        bool doCreateTests(std::ostream& anOutput) const {
            ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/test");
            if (theArchive.isOK()) {
                if (!fs::exists(folder + "/test.arc")) { return false; }
            }
            else { return false; }

            ArchiveStatus<std::shared_ptr<Archive>> theArchive2 = Archive::createArchive(folder + "/test");
            if (theArchive2.isOK()) {
                if (!fs::exists(folder + "/test.arc")) { return false; }
            }
            else { return false; }

            ArchiveStatus<std::shared_ptr<Archive>> theArchive3 = Archive::createArchive(folder + "/test2");
            if (theArchive3.isOK()) {
                if (!fs::exists(folder + "/test2.arc")) { return false; }
            }
            else { return false; }
            return true;
        }

        //-------------------------------------------

        bool doOpenTests(std::ostream& anOutput) {

            ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/test");
            if (theArchive.isOK()) {
                theArchive.getValue()->add(folder + "/smallA.txt");
                ArchiveStatus<std::shared_ptr<Archive>> newArchive = Archive::openArchive(folder + "/test");
                if (newArchive.isOK()) {
                    std::fstream testFileStream;
                    testFileStream.open(folder + "/test.arc");
                    if (testFileStream.fail()) { return false; }
                }
            }
            return true;
        }

        size_t addTestFile(Archive& anArchive,
                           const std::string& aName, char aChar = 'A') {
            std::string theFullPath(folder + '/' + aName + aChar + ".txt");
            anArchive.add(theFullPath);
            return 1;
        }

        size_t addTestFiles(Archive& anArchive, char aChar = 'A') {
            addTestFile(anArchive, "small", aChar);
            addTestFile(anArchive, "medium", aChar);
            addTestFile(anArchive, "large", aChar);
            return 3;
        }

        size_t getFileSize(const std::string& aFilePath) {
            std::ifstream theStream(aFilePath, std::ios::binary);
            const auto theBegin = theStream.tellg();
            theStream.seekg(0, std::ios::end);
            const auto theEnd = theStream.tellg();
            return theEnd - theBegin;
        }

        bool hasMinSize(const std::string& aFilePath, size_t aMinSize) {
            size_t theSize = getFileSize(aFilePath);
            auto theTest = theSize - aMinSize;
            return (theSize - aMinSize) <= kMinBlockSize; //ok to have one extra block
        }

        size_t countLines(const std::string &anOutput) {
            std::stringstream theOutput(anOutput);
            std::string theLine;
            size_t theCount{0};

            while(getline(theOutput,theLine)) {
                std::cout << theLine << "\n";
                theCount++;
            }
            return theCount-2;
        }

        bool doAddTests(std::ostream& anOutput) {
            bool    theResult = false;
            size_t  theCount = 0;
            size_t  theAddCount = 0;

            std::string theFullPath(folder+"/addtest.arc");
            { // block to limit scope of theArchive...
                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(theFullPath);
                if (theArchive.isOK()) {
                    std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                    theArchive.getValue()->addObserver(theObserver);
                    theAddCount = addTestFiles(*theArchive.getValue());
                }
                else{
                    anOutput << "Failed to create archive\n";
                    return false;
                }
            }
            {
                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::openArchive(theFullPath);
                if(theArchive.isOK()) {
                    std::stringstream theStream;
                    theArchive.getValue()->list(theStream);
                    theCount=countLines(theStream.str());
                }
                else{
                    anOutput << "Failed to open archive\n";
                    return false;
                }
            }
            if (theCount == 0 || theCount != theAddCount) {
                anOutput << "Archive doesn't have enough elements\n";
                return false;
            }
            else {
                theResult = hasMinSize(theFullPath, 6144);
                if(!theResult) anOutput << "Archive is too small\n";
            }
            return theResult;
        }

        //-------------------------------------------

        //why is large.text rather than large.txt?
        std::string pickRandomFile(char aChar='A') {
            static const char* theFiles[] = { "small","medium","large" };
            size_t theCount = sizeof(theFiles) / sizeof(char*);
            const char* theName=theFiles[rand() % theCount];
            std::string theResult(theName);
            theResult+=aChar;
            theResult+=".txt";
            return theResult;
        }

        bool filesMatch(const std::string& aFilename, const std::string& aFullPath) {
            std::string theFilePath(folder+"/"+aFilename);
            std::ifstream theFile1(theFilePath);
            std::ifstream theFile2(aFullPath);

            std::string theLine1;
            std::string theLine2;

            bool theResult{true};
            while (theResult && std::getline(theFile1, theLine1)) {
                std::getline(theFile2, theLine2);
                theResult=theLine1==theLine2;
            }
            return true;
        }

        bool doExtractTests(std::ostream& anOutput) {
            auto& theTracker = Tracker::instance();
            theTracker.enable(true).reset();
            {
                bool theResult = false;
                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/extracttest");
                if (theArchive.isOK()) {
                    std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                    theArchive.getValue()->addObserver(theObserver);
                    addTestFiles(*theArchive.getValue());
                    std::stringstream theStream;
                    std::string theFileName = pickRandomFile();
                    std::string temp(folder + "/out.txt");
                    theArchive.getValue()->extract(theFileName, temp);
                    theResult = filesMatch(theFileName, temp);
                }
                else{
                    anOutput << "Failed to create archive\n";
                    return false;
                }

                if (!theResult) {
                    anOutput << "Extracted file doesn't match original.\n";
                    return theResult;
                }
            }
            theTracker.reportLeaks(anOutput);
            return true;
        }

        //-------------------------------------------

        bool verifyRemove(const std::string& aName, size_t aCount, std::string& anOutput) {
            std::stringstream theInput(anOutput);
            std::string theName;
            char theBuffer[512];
            size_t theCount=0;

            while (!theInput.eof()) {
                theInput.getline(theBuffer, sizeof(theBuffer));
                if (strlen(theBuffer)) {
                    if(theBuffer[0]!='-' && theBuffer[0]!='#') {
                        std::string temp(theBuffer);
                        std::stringstream theLineInput(temp);
                        theLineInput >> theName >> theName;
                        if (theName == aName) return false;
                        theCount++;
                    }
                }
            }

            return aCount==theCount;
        }

        bool doRemoveTests(std::ostream& anOutput) {
            bool theResult = false;
            std::string theFileName;
            {
                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/test");
                if (theArchive.isOK()) {
                    std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                    theArchive.getValue()->addObserver(theObserver);
                    addTestFiles(*theArchive.getValue());
                    theFileName = pickRandomFile();
                    theArchive.getValue()->remove(theFileName);
                }
                else{
                    anOutput << "Failed to create archive\n";
                    return false;
                }
            }

            {
                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::openArchive(folder + "/test");
                if(theArchive.isOK()) {
                    std::stringstream theStream;
                    theArchive.getValue()->list(theStream);
                    std::string theOutput = theStream.str();
                    theResult = verifyRemove(theFileName, 2, theOutput);
                    if(!theResult) anOutput << "remove file failed\n";
                }
                else{
                    anOutput << "Failed to open archive\n";
                    return false;
                }
            }

            return theResult;
        }

        //-------------------------------------------

        bool verifyAddList(const std::string& aString) {
            std::map<std::string, size_t> theCounts;
            std::stringstream theInput(aString);
            char theBuffer[512];
            while (!theInput.eof()) {
                theInput.getline(theBuffer, sizeof(theBuffer));
                if (strlen(theBuffer)) {
                    std::string temp(theBuffer);
                    std::stringstream theLineInput(temp);
                    std::string theName;
                    theLineInput >> theName >> theName;
                    std::string thePrefix = theName.substr(0, 3);
                    theCounts[thePrefix] += 1;
                }
            }
            bool theResult = (theCounts["sma"] == 2) &&
                             (theCounts["med"] == 2) &&
                             (theCounts["lar"] == 2);
            return theResult;
        }

        bool doListTests(std::ostream& anOutput) {
            bool theResult = false;
            ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/test");
            if (theArchive.isOK()) {
                std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                theArchive.getValue()->addObserver(theObserver);
                addTestFiles(*theArchive.getValue());
                addTestFiles(*theArchive.getValue(), 'B');

                theArchive = Archive::openArchive(folder + "/test");
                std::stringstream theStream;
                theArchive.getValue()->list(theStream);
                std::string theOutput = theStream.str();
                theResult = verifyAddList(theOutput);
                if(!theResult) anOutput << "lists didn't match!\n";
            }
            return theResult;
        }

        //-------------------------------------------

        bool verifyDump(const std::string& aString) {
            std::map<std::string, size_t> theCounts;
            //std::cout << aString << "\n"; //debug

            std::stringstream theInput(aString);
            char theBuffer[512];
            while (!theInput.eof()) {
                theInput.getline(theBuffer, sizeof(theBuffer));
                if (strlen(theBuffer)) {
                    std::string temp(theBuffer);
                    std::stringstream theLineInput(temp);
                    std::string theName;
                    theLineInput >> theName >> theName >> theName;
                    std::string thePrefix = theName.substr(0, 3);
                    theCounts[thePrefix] += 1;
                }
            }
            bool theResult = (theCounts["sma"] == 2) &&
                             (theCounts["med"] == 4) &&
                             (theCounts["lar"] == 6);
            return theResult;
        }

        bool doDumpTests(std::ostream& anOutput) {
            bool theResult = false;
            auto& theTracker = Tracker::instance();
            theTracker.enable(true).reset();
            {
                {
                    ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(folder + "/dumptest");
                    if (theArchive.isOK()) {
                        std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                        theArchive.getValue()->addObserver(theObserver);
                        addTestFiles(*theArchive.getValue(), 'A');
                        addTestFiles(*theArchive.getValue(), 'B');
                        std::stringstream theStream;
                        if (12 == theArchive.getValue()->debugDump(theStream).getValue()) {
                            theResult = verifyDump(theStream.str());
                        }
                    }
                    else{
                        anOutput << "Failed to create archive\n";
                        return false;
                    }
                }

                if (theResult) {
                    std::string theArcName(folder + "/dumptest.arc");
                    theResult = hasMinSize(theArcName, 1024 * 12);
                }
            }
            theTracker.reportLeaks(anOutput);
            return theResult;
        }

        //-------------------------------------------

        bool verifyStressList(Archive& anArchive) {
            std::stringstream theStream;
            size_t            theCount = 0;
            std::string       theName;

            anArchive.list(theStream);

            std::stringstream theInput(theStream.str());
            char theBuffer[512];
            while (!theInput.eof()) {
                theInput.getline(theBuffer, sizeof(theBuffer));
                if (strlen(theBuffer)) {
                    std::string temp(theBuffer);
                    std::stringstream theLineInput(temp);
                    theLineInput >> theName >> theName;

                    auto theIter = std::find(
                            stressList.begin(), stressList.end(), theName);

                    if (theIter != stressList.end()) {
                        theCount++; //we found the name in our list...
                    }
                }
            } //while

            return theCount == stressList.size(); //names match
        }

        //-------------------------------------------

        bool stressAdd(Archive& anArchive) {
            static int counter = 0;
            std::stringstream temp;
            temp << "fake" << ++counter << ".txt";
            std::string theName(temp.str());
            stressList.push_back(theName);
            const int theMin = 1000;
            size_t theSize = theMin + rand() % ((2001) - theMin);
            std::string theFullPath(folder + '/' + theName);
            makeFile(theFullPath, theSize);
            anArchive.add(theFullPath);
            return verifyStressList(anArchive);
        }

        //-------------------------------------------

        bool stressRemove(Archive& anArchive) {
            if (stressList.size()>1) {
                size_t theIndex = rand() % stressList.size();
                std::string theName = stressList[theIndex];
                stressList.erase(stressList.begin() + theIndex);
                anArchive.remove(theName);
                return verifyStressList(anArchive);
            }
            return true;
        }

        //-------------------------------------------

        bool stressExtract(Archive& anArchive) {
            if (stressList.size()) {
                std::string theOutFileName(folder + "/out.txt");
                size_t theIndex = rand() % stressList.size();
                std::string theName = stressList[theIndex];
                anArchive.extract(theName, theOutFileName);
                return filesMatch(theName, theOutFileName);
            }
            return true;
        }

        //-------------------------------------------
        //return block count...
        size_t doStressDump(Archive& anArchive, size_t& aFreeCount) {
            size_t theBlockCount = 0;
            if (stressList.size()) {
                std::stringstream theOutput;
                anArchive.debugDump(theOutput);

                std::stringstream theInput(theOutput.str());
                char theBuffer[512];
                std::string theStatus;
                std::string theName;

                while (!theInput.eof()) {
                    theInput.getline(theBuffer, sizeof(theBuffer));
                    if (strlen(theBuffer)) {
                        std::string temp(theBuffer);
                        std::stringstream theLineInput(temp);
                        theLineInput >> theName >> theStatus >> theName;

                        auto theIter = std::find(stressList.begin(), stressList.end(), theName);

                        if (theStatus == "empty") {
                            aFreeCount++;
                            theBlockCount++;
                        }
                        else if (theIter != stressList.end()) {
                            theBlockCount++;
                        }
                    }
                }
            }
            return theBlockCount;
        }

        //-------------------------------------------

        bool doStressTests(std::ostream& anOutput) {
            bool theResult = true;

            std::string thePath(folder + "/stresstest.arc");
            if (ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::createArchive(thePath); theArchive.getValue()) {
                std::shared_ptr<ArchiveObserver> theObserver = std::make_shared<Testing>(*this);
                theArchive.getValue()->addObserver(theObserver);
                addTestFiles(*theArchive.getValue(), 'B'); //bootstrap...
                stressList.push_back("smallB.txt");
                stressList.push_back("mediumB.txt");
                stressList.push_back("largeB.txt");

                size_t theOpCount = 500;
                static ActionType theCalls[] = {
                        ActionType::added,
                        ActionType::removed,
                        ActionType::extracted
                };

                while (theResult && theOpCount--) {
                    switch (theCalls[rand() % 3]) {
                        case ActionType::added:
                            theResult = stressAdd(*theArchive.getValue());
                            break;
                        case ActionType::removed:
                            theResult = stressRemove(*theArchive.getValue());
                            break;
                        case ActionType::extracted:
                            theResult = stressExtract(*theArchive.getValue());
                            break;
                        default:
                            break;
                    }
                } //while
            }
            std::cout << "Done with stress operations" << std::endl;
            if (theResult) {

                ArchiveStatus<std::shared_ptr<Archive>> theArchive = Archive::openArchive(thePath);

                size_t thePreFreeCount = 0;
                size_t thePreCount = doStressDump(*theArchive.getValue(), thePreFreeCount);
                size_t thePreSize = getFileSize(thePath);

                //Final test: Dump, compact, re-dump and compare...
                theResult=false;
                if (theArchive.isOK() && thePreCount && thePreSize) {

                    if (auto theBlockCount = theArchive.getValue()->compact(); theBlockCount.getValue()) {
                        size_t thePostFreeCount = 0;
                        size_t thePostCount = doStressDump(*theArchive.getValue(), thePostFreeCount);
                        theResult = (thePostCount <= thePreCount) && (thePostFreeCount <= thePreFreeCount);
                    }

                    if (theResult) { //compacted file should be smaller...
                        size_t thePostSize = getFileSize(thePath);
                        theResult = thePostSize <= thePreSize;
                    }
                }
            } //if

            return theResult;
        }

        void operator()(ActionType anAction, const std::string& aName, bool status) {
            std::cerr << "observed ";
            switch (anAction) {
                case ActionType::added: std::cerr << "add "; break;
                case ActionType::extracted: std::cerr << "extract "; break;
                case ActionType::removed: std::cerr << "remove "; break;
                case ActionType::listed: std::cerr << "list "; break;
                case ActionType::dumped: std::cerr << "dump "; break;
                case ActionType::compacted: std::cerr << "compact "; break;
            }
            std::cerr << aName << "\n";
        }

    };


}

#endif /* Testing_h */
