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
#include <zlib.h>

namespace ECE141 {

    const size_t kBlockSize=1024;
    const size_t kFileNameSize = 30;
    const size_t kProcessorTypeNameSize = 5; // processor name MUST be only length 4 so that null terminator is not affected
    const char nullChar = '\0';

    enum class ActionType {added, extracted, removed, listed, dumped, compacted};
    enum class AccessMode {AsNew, AsExisting}; //you can change values (but not names) of this enum
    enum class StreamType {Archive, NonArchive};
    enum class ProcessorType {Compression};

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

    size_t getStreamNumBlocks(std::fstream& aStream, StreamType theStreamType=StreamType::Archive);

    struct TOC{
        TOC() = default;
        // maps hash of a block's filepath to its block index (use with seekg to read block's data)
        std::map<std::string, size_t> mapTOC;
        // hashes the block's filepath and inserts into map above
        void addBlockMeta(std::string blockFilePath, size_t theIndex);
        size_t getBlockIndex(std::string blockFilePath);
    };

    struct Header{
        Header();
        size_t blockIndex;
        size_t nextBlockIndex;
        size_t blockDataLen;
        bool isEmpty;
        bool isProcessed;
        char processorType[kProcessorTypeNameSize];
        char blockFileName[kFileNameSize];
    };

    constexpr size_t headerSize = sizeof(Header);

    struct Block {
        Block() = default;
        Header header;
        char data[kBlockSize - headerSize];
    };

    class Archive; // forward declare

    struct BlockHandler {
        BlockHandler() = default;
        /* Makes a block (with complete header initialization) corresponding to a 1024 byte section from archive file
         * Defers error handling to caller
         */
        ArchiveStatus<Block> getAsBlock(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos,
                                        Archive& theArchive, StreamType theStreamType);
        bool isBlockEmpty(Block &aBlock, size_t aPos);
        // iterate over blocks and return indexes of empty blocks
        std::vector<Block> getEmptyBlocks(Archive& theArchive);
        std::vector<Block> getProcessedBlocks(Archive& theArchive);
        ArchiveStatus<Block> writeToStream(Block &aBlock, size_t streamPos, std::fstream& anFStream, size_t arcPos,
                                           Archive& theArchive, StreamType theDestinationStreamType);
        ProcessorType getProcessorType(const char* processorName);
    };

    class IDataProcessor {
    public:
        virtual ArchiveStatus<bool> process(const std::string &aFilename) = 0;
        virtual ArchiveStatus<bool> reverseProcess(const std::string &aFilename) = 0;
        virtual ~IDataProcessor(){};
    };

    /** This is new child class of data processor, use it to compress the if add asks for it*/
    class Compression : public IDataProcessor {
    public:
        ArchiveStatus<bool> process(const std::string &aFilename) override {
            int ret, flush;
            unsigned have;
            z_stream strm;
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
            if (ret != Z_OK){
                std::cerr << "deflateInit failed\n";
                return ArchiveStatus<bool>(false);
            }
            auto CHUNK = kBlockSize - headerSize;
            unsigned char in[CHUNK];
            unsigned char out[CHUNK];
            std::string destFilePath{aFilename};
            destFilePath.insert(aFilename.length()-4,"_processed");
            FILE *source = fopen(aFilename.c_str(), "rb"); // open in read mode
            FILE *dest = fopen( destFilePath.c_str(),"wb");
            auto theErr = strerror(errno);
            do {
                strm.avail_in = fread(in, 1, CHUNK, source);
                if (ferror(source)) {
                    (void)deflateEnd(&strm);
                    return ArchiveStatus<bool>(false);
                }
                flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
                strm.next_in = in;

                do{
                    strm.avail_out = CHUNK;
                    strm.next_out = out;
                    ret = deflate(&strm, flush);
                    have = CHUNK - strm.avail_out;
                    if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                        (void)deflateEnd(&strm);
                        return ArchiveStatus<bool>(false);
                    }
                } while (strm.avail_out == 0);
            } while (flush != Z_FINISH);
            (void)deflateEnd(&strm);
            fclose(dest);
            fclose(source);

            return ArchiveStatus<bool>(true);
        }

        // takes in the filepath of the file with compressed data and creates a new file with uncompressed data
        ArchiveStatus<bool> reverseProcess(const std::string &aFilename) override {
            int ret;
            unsigned have;
            z_stream strm;
            auto CHUNK = kBlockSize - headerSize;
            unsigned char in[CHUNK];
            unsigned char out[CHUNK];
            std::string sourceFilePath{aFilename};
            sourceFilePath.insert(aFilename.length()-4,"_reverse_process");
            FILE *source = fopen(sourceFilePath.c_str(), "rb"); // open in read mode
            FILE *dest = fopen( aFilename.c_str(),"wb");
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.avail_in = 0;
            strm.next_in = Z_NULL;
            ret = inflateInit(&strm);
            if (ret != Z_OK){return ArchiveStatus<bool>(false);}
            do {
                strm.avail_in = fread(in, 1, CHUNK, source);
                if (ferror(source)) {
                    (void)inflateEnd(&strm);
                    return ArchiveStatus<bool>(false);;
                }
                if (strm.avail_in == 0)
                    break;
                strm.next_in = in;

                do{
                    strm.avail_out = CHUNK;
                    strm.next_out = out;
                    ret = inflate(&strm, Z_NO_FLUSH);
                    switch (ret) {
                        case Z_NEED_DICT:
                            ret = Z_DATA_ERROR;     /* and fall through */
                        case Z_DATA_ERROR:
                        case Z_MEM_ERROR:
                            (void)inflateEnd(&strm);
                            return ArchiveStatus<bool>(false);;
                    }
                    have = CHUNK - strm.avail_out;
                    if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                        (void)inflateEnd(&strm);
                        return ArchiveStatus<bool>(false);;
                    }
                } while (strm.avail_out == 0);
            } while (ret != Z_STREAM_END);
            (void)inflateEnd(&strm);
            fclose(source);
            fclose(dest);

            return ArchiveStatus<bool>(true);
        }

        ~Compression() override = default;
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

        ArchiveStatus<bool>      add(const std::string &aFilename, IDataProcessor* aProcessor=nullptr);
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath);
        ArchiveStatus<bool>      remove(const std::string &aFilename);

        ArchiveStatus<bool>      resize(size_t aBlockSize); // New!
        ArchiveStatus<bool>      merge(const std::string &anArchiveName); // New!
        ArchiveStatus<bool>      addFolder(const std::string &aFolder); // New!
        ArchiveStatus<bool>      extractFolder(const std::string &aFolderName, const std::string &anExtractPath); // New!

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
