//
//  Archive.cpp
//

#include "Archive.hpp"

namespace ECE141 {

    Archive::Archive(const std::string &aFullPath, AccessMode aMode){
        auto newFileMode = std::fstream::binary | std::fstream::in | std::fstream::out | std::fstream::trunc;
        auto existingFileMode = std::fstream::binary | std::fstream::in | std::fstream::out | std::fstream::app;
        arcPath = aFullPath;
        if(aFullPath.find(".arc") == std::string::npos){
            arcPath = aFullPath + ".arc";
        }
        switch(aMode){
            case AccessMode::AsNew:
                arcFileStream.open(arcPath, newFileMode);
                arcNumBlocks = 0;
                break;
            case AccessMode::AsExisting:
                arcFileStream.open(arcPath, existingFileMode);
                if(!arcFileStream.is_open()){throw std::runtime_error("Failed to open archive");}
                arcNumBlocks = getStreamNumBlocks(arcFileStream) - 1;
                reconstructTOC();
                break;
        }
        arcFolder = static_cast<std::filesystem::path>(arcPath).parent_path();
    }

    Archive::~Archive(){
        if(arcFileStream.is_open()){arcFileStream.close();}
    }

    void Archive::reconstructTOC() {
        for(size_t i=0; i<arcNumBlocks; i++){
            Block aBlock;
            arcBlockHandler.getAsBlock(aBlock, 0, arcFileStream, i, *this, StreamType::Archive);
            if(!aBlock.header.isEmpty){
                arcTOC.mapTOC.insert(std::pair<std::string, size_t>(std::string(aBlock.header.blockFileName), aBlock.header.blockIndex));
            }
        }
    }

    ArchiveStatus<std::shared_ptr<Archive>> Archive::createArchive(const std::string &anArchiveName){
        auto theArcPtr = std::shared_ptr<Archive>(new Archive(anArchiveName, AccessMode::AsNew));
        auto theStatus = ArchiveStatus(theArcPtr);
        return theStatus;
    }

    ArchiveStatus<std::shared_ptr<Archive>> Archive::openArchive(const std::string &anArchiveName) {
        try{
            auto theArcPtr = std::shared_ptr<Archive>(new Archive(anArchiveName, AccessMode::AsExisting));
            return ArchiveStatus<std::shared_ptr<Archive>>(theArcPtr);
        }
        catch(std::runtime_error& e){
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileOpenError);
        }
    }

    void Archive::notifyObservers(ActionType anAction, const std::string &aName, bool status){
        for(auto& observer: arcObservers){
            observer->operator()(anAction, aName, status);
        }
    }

    size_t getStreamNumBlocks(std::fstream& aStream, StreamType theStreamType){
        aStream.seekp(0, std::ios::end); // set ptr to end
        size_t fileLen = aStream.tellp();
        size_t numBlocksNeeded;
        switch (theStreamType){
            case StreamType::Archive:
                numBlocksNeeded = (fileLen / kBlockSize) + 1;
                break;
            case StreamType::NonArchive:
                numBlocksNeeded = (fileLen / (kBlockSize - headerSize)) + 1;
        }

        aStream.seekp(0, std::ios::beg); // reset to beginning
        return numBlocksNeeded;
    }

    std::vector<Block> BlockHandler::getProcessedBlocks(Archive& theArchive){
        std::vector<Block> processedBlocks;
        theArchive.arcFileStream.seekp(0, std::ios::end); // set ptr to end
        for(size_t i=0; i<theArchive.arcNumBlocks; i++){
            Block aBlock;
            getAsBlock(aBlock, 0, theArchive.arcFileStream, i, theArchive, StreamType::Archive);
            if(aBlock.header.isProcessed){
                processedBlocks.push_back(aBlock);
            }
        }
        return processedBlocks;
    }

    Header::Header() : blockIndex(-1), nextBlockIndex(-1), blockDataLen(0), isEmpty(false), isProcessed(false)
    {
        std::memset(blockFileName, nullChar, sizeof(blockFileName));
        std::memset(processorType, nullChar, sizeof(processorType));
    }

    ProcessorType BlockHandler::getProcessorType(const char* processorName){
        std::map<const char*, ProcessorType> theProcessorMap = {
                {"comp",ProcessorType::Compression}
        };
        return theProcessorMap[processorName];
    }

    ArchiveStatus<Block> BlockHandler::getAsBlock(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos, Archive& theArchive, StreamType theStreamType) {
        auto theTest= sizeof(aBlock);
        if(theStreamType == StreamType::Archive){
            theArchive.arcFileStream.seekp(arcPos * kBlockSize); // at the header
            auto currLoc = theArchive.arcFileStream.tellp();
            theArchive.arcFileStream.read(reinterpret_cast<char *>(&aBlock), sizeof(aBlock));
            theArchive.arcFileStream.clear();
        }
        else{ // in a normal filestream, there is no header data
            // first fill the block data with nulls so that there is no undefined behaviour
            std::memset(aBlock.data, nullChar, kBlockSize - sizeof(aBlock.header));
            auto theStartPtrloc = anFStream.tellp();
            anFStream.read((char *) aBlock.data, kBlockSize - sizeof(aBlock.header));
            aBlock.header.blockDataLen = anFStream.gcount();
            aBlock.header.isEmpty = false; // note: this is already false when the Header object is initialized
            // , so we don't need to explicitly set any indicators to false e.g. isCompressed
            anFStream.clear();
        }
        return ArchiveStatus<Block>(aBlock);
    }

    bool BlockHandler::isBlockEmpty(Block &aBlock, size_t aPos){
        return aBlock.header.isEmpty;
    }

    std::vector<Block> BlockHandler::getEmptyBlocks(Archive& theArchive){
        std::vector<Block> emptyBlocks;
        bool allFound = false;
        for(size_t thePos=0; thePos<theArchive.arcNumBlocks; thePos++){
            Block aBlock;
            auto theStatus = getAsBlock(aBlock, thePos, theArchive.arcFileStream, thePos, theArchive,
                                        StreamType::Archive);
            aBlock = theStatus.getValue();
            if(isBlockEmpty(aBlock, thePos)){
                emptyBlocks.push_back(aBlock);
            }
        }
        return emptyBlocks;
    }

    ArchiveStatus<Block> BlockHandler::writeToStream(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos, Archive& theArchive, StreamType theDestinationStreamType){
        // when writing to archive, explicitly cast all metadata to string first. BlockFileName is already initialized with nulls
        auto headerSize = sizeof(aBlock.header);
        if(theDestinationStreamType == StreamType::Archive) {
            theArchive.arcFileStream.seekp(arcPos * kBlockSize);
            theArchive.arcFileStream.write(reinterpret_cast<const char*>(&aBlock),sizeof(aBlock));
            theArchive.arcFileStream.clear();
        }
        else {
            // only write blockDataLen amount of data (i.e. don't write padding characters)
            // and don't change pointer settings as this is variable length
            auto theLoc = anFStream.tellp();
            bool theRes = anFStream.fail();
            anFStream.write((const char*) aBlock.data, aBlock.header.blockDataLen);
            auto theCount = anFStream.gcount();
            anFStream.clear();
        }
        return ArchiveStatus<Block>(aBlock);
    }

    void TOC::addBlockMeta(std::string blockFileName, size_t theIndex){
//        std::size_t pathHash = std::hash<std::string>{}(blockFileName);
        // note, if multiple blocks per file, TOC map only stores index of first block; use block header to find next
        mapTOC.insert(std::pair<std::string, size_t>(blockFileName, theIndex));
    }

    size_t TOC::getBlockIndex(std::string blockFilePath){
        return mapTOC[blockFilePath];
    }

    ArchiveStatus<bool> Archive::add(const std::string &aFilename, IDataProcessor* aProcessor){
        // check that a file with the same name doesn't already exist
        if(arcTOC.mapTOC.find(aFilename) != arcTOC.mapTOC.end()) {
            return ArchiveStatus<bool>(ArchiveErrors::fileExists);
        }
        auto theFileMode = std::fstream::binary |
                             std::fstream::in | std::fstream::out;
        std::fstream theStream;
        theStream.open(aFilename, theFileMode);
        theStream.seekp(0);

        // --------------- Processing  ----------------

        // processing opens the input file to compress and saves processed result to temp output file, which getAsBlock will read in below
        if(aProcessor){
            aProcessor->process(aFilename);
            // now update the stream to be the processed file rather than the original input file
            theStream.close();
            std::string destFilePath{aFilename};
            destFilePath.insert(aFilename.length()-4,"_processed");
            theStream.open(destFilePath, theFileMode);
            // NOTE: the blocks metadata for isProcessed is updated below just before getAsBlock
        }
        // ---------------- End processing  ------------

        size_t numBlocksNeeded = getStreamNumBlocks(theStream, StreamType::NonArchive);

        // get the empty blocks first
        std::vector<Block> emptyBlocks = arcBlockHandler.getEmptyBlocks(*this);

        for(size_t i=0; i<numBlocksNeeded; i++){
            // initialize as empty block to append at end of archive, overwrite if empty blocks exist
            Block theBlock;
            size_t thePos{arcNumBlocks};
            theBlock.header.blockIndex = thePos;
            if(i < numBlocksNeeded - 1){ theBlock.header.nextBlockIndex = thePos + 1; }
            else{ theBlock.header.nextBlockIndex = thePos; }
            arcNumBlocks++;

            std::strcpy(theBlock.header.blockFileName, aFilename.c_str()); // whether its empty or new block, the filename should be explicitly set

            // ----------------- Processing ----------------------
            if(aProcessor) {
                theBlock.header.isProcessed = true;
                // figure out which processor was called and enter the name into header.processorType
                if(typeid(*aProcessor) == typeid(Compression)){
                    std::strcpy(theBlock.header.processorType, "comp");
                }
            }
            auto theStatus = arcBlockHandler.getAsBlock(theBlock, i, theStream, thePos, *this,
                                                        StreamType::NonArchive);

            // blockDataLen is updated inside getAsBlock so when writing to stream, we only write that much data
            theStatus.getValue(); // does error checking
            auto theWriteStatus = arcBlockHandler.writeToStream(theBlock, i,theStream,
                                                                thePos, *this, StreamType::Archive);
            if(!theWriteStatus.isOK()){
                notifyObservers(ActionType::added, aFilename, false);
                return ArchiveStatus<bool>(false);
            }
            std::string nonConstName = std::string(aFilename);
            arcTOC.addBlockMeta(nonConstName, thePos);
        }
        arcFileStream.seekp(0,std::ios::beg);
        theStream.close();
        notifyObservers(ActionType::added, aFilename, true);
        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<bool> Archive::extract(const std::string &aFilename, const std::string &aFullPath){
        // lookup filename in TOC, then iterate over all linked blocks using header.nextBlockIndex
        auto theFileMode = std::fstream::binary | std::fstream::out;
        std::fstream theStream;
        theStream.open(aFullPath, theFileMode);
        auto theLoc = theStream.tellp();
        auto theLoc2 = theStream.tellg();
        auto fullFilenamePath = aFilename;
        if(fullFilenamePath.find(arcFolder) == std::string::npos){ fullFilenamePath = arcFolder + "/" + aFilename; }
        // get the starting block index of all blocks that contain the file that is requested
        auto blockIndex = arcTOC.getBlockIndex(fullFilenamePath);
        std::string destFilePath{aFullPath};
        int c = 0;
        while(true){
            Block theBlock;
            auto theStatus = arcBlockHandler.getAsBlock(theBlock, blockIndex,
                                                        arcFileStream, blockIndex, *this, StreamType::Archive);
            theStatus.getValue(); // does error checking

            // if the block was processed, write it to a temp file, then (later) read that in, reverse the processing,
            // and write to the desired input filename
            if(theBlock.header.isProcessed && c==0) {
                theStream.close();
                destFilePath.insert(aFullPath.length()-4,"_reverse_process");
                theStream.open(destFilePath, theFileMode);
                theStream.seekp(0,std::ios::beg);
            }

            // streamPos doesn't need to be updated as it simply keeps moving along the stream as data is written to it
            auto theWriteStatus = arcBlockHandler.writeToStream(theBlock, 0,
                                                                theStream, blockIndex, *this, StreamType::NonArchive);
            // terminating condition is if there is no next block for the requested file
            if(theBlock.header.nextBlockIndex == theBlock.header.blockIndex){

                //------------------ Reverse Processing --------------------
                // if a file was processed when adding, find which processor was called and reverse the processing
                if(theBlock.header.isProcessed) {
                    IDataProcessor *aProcessor = nullptr;
                    ProcessorType processorType = arcBlockHandler.getProcessorType(
                            theBlock.header.processorType); // returns enum class type
                    switch (processorType) {
                        case ProcessorType::Compression:
                            aProcessor = new Compression();
                            break;
                    }
                    theStream.close();
                    aProcessor->reverseProcess(aFullPath);
                    delete aProcessor;
                }
                //----------------- End reverse processing -------------------

                notifyObservers(ActionType::extracted, aFilename, true);
                theStream.close();
                return ArchiveStatus<bool>(true);
            }
            blockIndex = theBlock.header.nextBlockIndex;
            c++;
        }
        notifyObservers(ActionType::extracted, aFilename, false);
        return ArchiveStatus<bool>(ArchiveErrors::fileWriteError);
    }

    ArchiveStatus<bool> Archive::remove(const std::string &aFilename){
        auto fullFilenamePath = aFilename;
        if(fullFilenamePath.find(arcFolder) == std::string::npos){ fullFilenamePath = arcFolder + "/" + aFilename; }
        auto blockIndex = arcTOC.getBlockIndex(fullFilenamePath);
        bool allLinkedVisited = false;
        while(!allLinkedVisited){
            Block theBlock;
            auto theStatus = arcBlockHandler.getAsBlock(theBlock, blockIndex, arcFileStream, blockIndex, *this, StreamType::Archive);
            theStatus.getValue();
            theBlock.header.isEmpty = true;
            theBlock.header.blockDataLen = 0;
//            std::memset(theBlock.header.blockFileName, nullChar, sizeof(theBlock.header.blockFileName));
            auto theWriteStatus = arcBlockHandler.writeToStream(theBlock, 0, arcFileStream, blockIndex, *this, StreamType::Archive);
            if(theBlock.header.nextBlockIndex == theBlock.header.blockIndex){
                notifyObservers(ActionType::removed, aFilename, true);
                arcTOC.mapTOC.erase(fullFilenamePath);
                return ArchiveStatus<bool>(true);
            }
            blockIndex = theBlock.header.nextBlockIndex;
            theBlock.header.nextBlockIndex = theBlock.header.blockIndex;
        }
        return ArchiveStatus<bool>(false);
    }

    ArchiveStatus<size_t> Archive::list(std::ostream &aStream){
        for(auto& element: arcTOC.mapTOC){
            auto parentPath = static_cast<std::filesystem::path>(element.first).parent_path();
            size_t pos = std::string(parentPath).size();
            std::string result = element.first.substr(pos + 1); // remove the / after the parent path
            aStream << std::string(result) << std::endl;
        }
        aStream << "#" << std::endl;
        aStream << "#" << std::endl;
        notifyObservers(ActionType::listed, std::string(""), true);
        return ArchiveStatus<size_t>(arcTOC.mapTOC.size());
    }

    ArchiveStatus<size_t> Archive::debugDump(std::ostream &aStream){
        size_t numBlocksArc = getStreamNumBlocks(arcFileStream) - 1;
        for(size_t thePos=0; thePos<numBlocksArc; thePos++){
            Block theBlock;
            auto theStatus = arcBlockHandler.getAsBlock(theBlock,0,arcFileStream,
                                                        thePos,*this,StreamType::Archive);

            auto parentPath = static_cast<std::filesystem::path>(theBlock.header.blockFileName).parent_path();
            size_t pos = std::string(parentPath).size();
//            std::cout << numBlocksArc << " " << theBlock.header.blockIndex << " " << theBlock.header.nextBlockIndex << " " << theBlock.header.blockFileName << " " << pos << std::endl;
            std::string fileName = std::string(theBlock.header.blockFileName).substr(pos+1);
            aStream << theBlock.header.blockIndex << " " << theBlock.header.isEmpty << " " << fileName << "\n";
        }

        notifyObservers(ActionType::dumped, std::string(""), true);
        return ArchiveStatus<size_t>(numBlocksArc);
    }

    ArchiveStatus<size_t> Archive::compact(){
        size_t numBlocksArc = getStreamNumBlocks(arcFileStream);
        std::fstream newArcFileStream;
        size_t ix = 0;
        for(size_t thePos=0; thePos<numBlocksArc; thePos++){
            Block theBlock;
            arcBlockHandler.getAsBlock(theBlock, thePos, arcFileStream, thePos, *this, StreamType::Archive);
            if(theBlock.header.isEmpty){}
            else{
                arcBlockHandler.writeToStream(theBlock, ix, newArcFileStream, ix, *this, StreamType::Archive);
                ix++;
            }
        }
        // overwrite the existing archive
        auto theFileMode = std::fstream::binary | std::fstream::in | std::fstream::out | std::fstream::trunc;
        newArcFileStream.open(arcPath, theFileMode);
        notifyObservers(ActionType::compacted, std::string(""), true);
        return ArchiveStatus<size_t>(ix);
    }

    Archive&  Archive::addObserver(std::shared_ptr<ArchiveObserver> anObserver){
        arcObservers.push_back(anObserver);
        return *this;
    }
}