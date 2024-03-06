//
//  Archive.hpp
//

#ifndef Archive_hpp
#define Archive_hpp

#include <cstdio>
#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <optional>
#include <stdexcept>
#include <filesystem>

namespace ECE141 {

    const size_t kBlockSize=1024;
    const size_t kFileNameSize = 30;
    const char nullChar = '\0';

    enum class ActionType {added, extracted, removed, listed, dumped, compacted}; // observers notified using these action types
    enum class AccessMode {AsNew, AsExisting}; //you can change values (but not names) of this enum
    enum class StreamType {Archive, NonArchive};

    struct ArchiveObserver {
        void operator()(ActionType anAction,
                        const std::string &aName, bool status){
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

    class IDataProcessor {
    public:
        virtual std::vector<uint8_t> process(const std::vector<uint8_t>& input) = 0;
        virtual std::vector<uint8_t> reverseProcess(const std::vector<uint8_t>& input) = 0;
    };

    enum class ArchiveErrors {
        noError=0,
        fileNotFound=1, fileExists, fileOpenError, fileReadError, fileWriteError, fileCloseError,
        fileSeekError, fileTellError, fileError, badFilename, badPath, badData, badBlock, badArchive,
        badAction, badMode, badProcessor, badBlockType, badBlockCount, badBlockIndex, badBlockData,
        badBlockHash, badBlockNumber, badBlockLength, badBlockDataLength, badBlockTypeLength
    };

    template<typename T>
    class ArchiveStatus {
    public:
        // Constructor for success case
        explicit ArchiveStatus(const T value)
                : value(value), error(ArchiveErrors::noError) {}

        // Constructor for error case
        explicit ArchiveStatus(ArchiveErrors anError)
                : value(std::nullopt), error(anError) {
            if (anError == ArchiveErrors::noError) {
                throw std::logic_error("Cannot use noError with error constructor");
            }
        }

        // Deleted copy constructor and copy assignment to make ArchiveStatus move-only
        ArchiveStatus(const ArchiveStatus&) = delete;
        ArchiveStatus& operator=(const ArchiveStatus&) = delete;

        // Default move constructor and move assignment
        ArchiveStatus(ArchiveStatus&&) noexcept = default;
        ArchiveStatus& operator=(ArchiveStatus&&) noexcept = default;

        T getValue() const {
            if (!isOK()) {
                throw std::runtime_error("Operation failed with error");
            }
            return *value;
        }

        bool isOK() const { return error == ArchiveErrors::noError && value.has_value(); }
        ArchiveErrors getError() const { return error; }

    private:
        std::optional<T> value;
        ArchiveErrors error;
    };

    size_t getStreamNumBlocks(std::fstream& aStream);

    struct TOC{
        TOC() = default;
        // maps hash of a block's filepath to its block index (use with seekg to read block's data)
        std::map<std::string, size_t> mapTOC;
        // hashes the block's filepath and inserts into map above
        void addBlockMeta(std::string blockFilePath, size_t theIndex);
        size_t getBlockIndex(std::string blockFilePath);
    };

    struct Header{
        Header(size_t _blockIndex=-1, size_t _nextBlockIndex=-1, size_t _blockDataLen=0, bool _isEmpty=false);
        size_t blockIndex;
        size_t nextBlockIndex;
        size_t blockDataLen;
        bool isEmpty;
        char blockFileName[kFileNameSize];
    };

    struct Block {
        Block() = default;
        Header header;
        char data[kBlockSize - sizeof(Header)];
    };

    class Archive; // forward declare

    struct BlockHandler {
        BlockHandler() = default;
        /* Makes a block (with complete header initialization) corresponding to a 1024 byte section from archive file
         * Defers error handling to caller
         */
        ArchiveStatus<Block> getAsBlock(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos, Archive& theArchive, StreamType theStreamType);
        bool isBlockEmpty(Block &aBlock, size_t aPos);
        // iterate over blocks and return indexes of empty blocks
        std::vector<Block> getEmptyBlocks(Archive& theArchive);
        ArchiveStatus<Block> writeToStream(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos, Archive& theArchive, StreamType theDestinationStreamType);

    };

    class Archive {
    protected:
        std::vector<std::shared_ptr<IDataProcessor>> processors; // keep this in mind when designing interface
        std::vector<std::shared_ptr<ArchiveObserver>> observers; // all of these need to be notified when any action is taken
        Archive(const std::string &aFullPath, AccessMode aMode);  //protected on purpose

    public:

        ~Archive();

        static    ArchiveStatus<std::shared_ptr<Archive>> createArchive(const std::string &anArchiveName);
        static    ArchiveStatus<std::shared_ptr<Archive>> openArchive(const std::string &anArchiveName);

        Archive&  addObserver(std::shared_ptr<ArchiveObserver> anObserver);
        void notifyObservers(ActionType anAction, const std::string &aName, bool status);

        ArchiveStatus<bool>      add(const std::string &aFilename);
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath);
        ArchiveStatus<bool>      remove(const std::string &aFilename);

        ArchiveStatus<size_t>    list(std::ostream &aStream);
        ArchiveStatus<size_t>    debugDump(std::ostream &aStream);

        ArchiveStatus<size_t>    compact();
        void reconstructTOC();

        TOC arcTOC;
        BlockHandler arcBlockHandler;
        std::string arcPath;
        std::fstream arcFileStream;
        size_t arcNumBlocks;
        std::vector<std::shared_ptr<ArchiveObserver>> arcObservers;
        std::string arcFolder;
    };

}

#endif /* Archive_hpp */
