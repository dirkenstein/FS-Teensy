/*
 FS.h - file system wrapper
 Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef FS_H
#define FS_H
#define FS_NO_GLOBALS
#include <memory>
#include <Arduino.h>
#include <map>
//#include <../include/time.h> // See issue #6714
#include <TimeLib.h> // See issue #6714

class SDClass;

#if defined (__AVR__)
// AVR fcntl.h does not define open flags.
#define USE_FCNTL_H 0
#elif defined(PLATFORM_ID)
// Particle boards - use fcntl.h.
#define USE_FCNTL_H 1
#elif defined(__arm__)
// ARM gcc defines open flags.
#define USE_FCNTL_H 1
#elif defined(ESP32) 
#define USE_FCNTL_H 1
#else  // defined(__AVR__)
#define USE_FCNTL_H 0
#endif  // defined(__AVR__)

#if USE_FCNTL_H
#include <fcntl.h>
/* values for GNU Arm Embedded Toolchain.
 * O_RDONLY:   0x0
 * O_WRONLY:   0x1
 * O_RDWR:     0x2
 * O_ACCMODE:  0x3
 * O_APPEND:   0x8
 * O_CREAT:    0x200
 * O_TRUNC:    0x400
 * O_EXCL:     0x800
 * O_SYNC:     0x2000
 * O_NONBLOCK: 0x4000
 */
/** Use O_NONBLOCK for open at EOF */
#define O_AT_END O_NONBLOCK  ///< Open at EOF.
namespace fs {
typedef int oflag_t;
}
#else  // USE_FCNTL_H
#warning using Arduino fcntl flags
#define O_RDONLY  0X00  ///< Open for reading only.
#define O_WRONLY  0X01  ///< Open for writing only.
#define O_RDWR    0X02  ///< Open for reading and writing.
#define O_AT_END  0X04  ///< Open at EOF.
#define O_APPEND  0X08  ///< Set append mode.
#define O_CREAT   0x10  ///< Create file if it does not exist.
#define O_TRUNC   0x20  ///< Truncate file to zero length.
#define O_EXCL    0x40  ///< Fail if the file exists.
#define O_SYNC    0x80  ///< Synchronized write I/O operations.

#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)  ///< Mask for access mode.
namespace fs {
typedef uint8_t oflag_t;
}
#endif  // USE_FCNTL_H

#define O_READ    O_RDONLY
#define O_WRITE   O_WRONLY

/** Arduino SD.h style flag for open for read. */
#ifndef FILE_READ
#define FILE_READ O_RDONLY
#endif  // FILE_READ
/** Arduino SD.h style flag for open at EOF for read/write with create. */
#ifndef FILE_WRITE
#define FILE_WRITE (O_RDWR | O_CREAT | O_AT_END)
#endif  // FILE_WRITE


enum OpenMode {
    OM_DEFAULT = 0,
    OM_CREATE = 1,
    OM_APPEND = 2,
    OM_TRUNCATE = 4
};

enum AccessMode {
    AM_READ = 1,
    AM_WRITE = 2,
    AM_RW = AM_READ | AM_WRITE
};

namespace fs {

class File;
class Dir;
class FS;

class FileImpl;
typedef std::shared_ptr<FileImpl> FileImplPtr;
class FSImpl;
typedef std::shared_ptr<FSImpl> FSImplPtr;
class DirImpl;
typedef std::shared_ptr<DirImpl> DirImplPtr;

typedef std::map<File*, File*> FileMap;

template <typename Tfs>
bool mount(Tfs& fs, const char* mountPoint);

inline bool isWriteMode(oflag_t oflag) {
  oflag &= O_ACCMODE;
  return oflag == O_WRONLY || oflag == O_RDWR;
}

enum SeekMode {
    SeekSet = 0,
    SeekCur = 1,
    SeekEnd = 2
};

class File : public Stream
{
public:

    File(FileImplPtr p = FileImplPtr(), FS *baseFS = nullptr);
    File(const File &oldf);
    ~File();
    // Print methods:
    size_t write(uint8_t) override;
    size_t write(const uint8_t *buf, size_t size) override;

    // Stream methods:
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t readBytes(char *buffer, size_t length) {
        return read((uint8_t*)buffer, length);
    }
    String readString();
    size_t read(uint8_t* buf, size_t size);
    bool seek(uint32_t pos, SeekMode mode);
    bool seek(uint32_t pos) {
        return seek(pos, SeekSet);
    }
    size_t position() const;
    size_t size() const;
    void close();
    operator bool() const;
    const char* name() const;
    const char* fullName() const; // Includes path
    bool truncate(uint32_t size);

    bool isFile() const;
    bool isDirectory() const;

    // Arduino "class SD" methods for compatibility
    template<typename T> size_t write(T &src){
      uint8_t obuf[256];
      size_t doneLen = 0;
      size_t sentLen;
      int i;

      while (src.available() > sizeof(obuf)){
        src.read(obuf, sizeof(obuf));
        sentLen = write(obuf, sizeof(obuf));
        doneLen = doneLen + sentLen;
        if(sentLen != sizeof(obuf)){
          return doneLen;
        }
      }

      size_t leftLen = src.available();
      src.read(obuf, leftLen);
      sentLen = write(obuf, leftLen);
      doneLen = doneLen + sentLen;
      return doneLen;
    }
    using Print::write;

    void rewindDirectory();
    File openNextFile();


    time_t getLastWrite();
    time_t getCreationTime();
    void setTimeCallback(time_t (*cb)(void));

protected:
    FileImplPtr _p;
    int _refcnt = 0;

    // Arduino SD class emulation
    std::shared_ptr<Dir> _fakeDir;
    FS                  *_baseFS;
};

class Dir {
public:
    Dir(DirImplPtr impl = DirImplPtr(), FS *baseFS = nullptr): _impl(impl), _baseFS(baseFS) { }

    File openFile(const char* mode);
    File openFile(oflag_t mode);
    File openFile(OpenMode om, AccessMode am);

    String fileName();
    size_t fileSize();
    time_t fileTime();
    time_t fileCreationTime();
    bool isFile() const;
    bool isDirectory() const;

    bool next();
    bool rewind();

    void setTimeCallback(time_t (*cb)(void));

protected:
    DirImplPtr _impl;
    FS       *_baseFS;
    time_t (*timeCallback)(void) = nullptr;
};

// Backwards compatible, <4GB filesystem usage
struct FSInfo {
    size_t totalBytes;
    size_t usedBytes;
    size_t blockSize;
    size_t pageSize;
    size_t maxOpenFiles;
    size_t maxPathLength;
};

// Support > 4GB filesystems (SD, etc.)
struct FSInfo64 {
    uint64_t totalBytes;
    uint64_t usedBytes;
    size_t blockSize;
    size_t pageSize;
    size_t maxOpenFiles;
    size_t maxPathLength;
};


class FSConfig
{
public:
    static constexpr uint32_t FSId = 0x00000000;

    FSConfig(uint32_t type = FSId, bool autoFormat = true) : _type(type), _autoFormat(autoFormat) { }

    FSConfig setAutoFormat(bool val = true) {
        _autoFormat = val;
        return *this;
    }

    uint32_t _type;
    bool     _autoFormat;
};

class SPIFFSConfig : public FSConfig
{
public:
    static constexpr uint32_t FSId = 0x53504946;
    SPIFFSConfig(bool autoFormat = true) : FSConfig(FSId, autoFormat) { }

    // Inherit _type and _autoFormat
    // nothing yet, enableTime TBD when SPIFFS has metadate
};

class FS
{
public:
    FS(FSImplPtr impl) : _impl(impl) { timeCallback = _defaultTimeCB; }

    bool setConfig(const FSConfig &cfg);

    bool begin();
    void end();

    bool format();
    bool info(FSInfo& info);
    bool info64(FSInfo64& info);

    File open(const char* path, const char* mode);
    File open(const char* path, oflag_t mode);
    File open(const String& path, const char* mode);
    File open(const String& path, oflag_t  mode);
    File open(const char* path, OpenMode om, AccessMode am);

    bool exists(const char* path);
    bool exists(const String& path);

    Dir openDir(const char* path);
    Dir openDir(const String& path);

    bool remove(const char* path);
    bool remove(const String& path);

    bool rename(const char* pathFrom, const char* pathTo);
    bool rename(const String& pathFrom, const String& pathTo);

    bool mkdir(const char* path);
    bool mkdir(const String& path);

    bool rmdir(const char* path);
    bool rmdir(const String& path);

    bool sync();
    // Low-level FS routines, not needed by most applications
    bool gc();
    bool check();

    void setTimeCallback(time_t (*cb)(void));

    friend class ::SDClass; // More of a frenemy, but SD needs internal implementation to get private FAT bits
    friend class File;
protected:
    FSImplPtr _impl;
    FSImplPtr getImpl() { return _impl; }
        
    time_t (*timeCallback)(void);
    static time_t _defaultTimeCB(void) { return now(); }
    FileMap _openFiles;
};

} // namespace fs

extern "C"
{
void close_all_fs(void);
void littlefs_request_end(void);
}

#ifndef FS_NO_GLOBALS
using fs::FS;
using fs::File;
using fs::Dir;
using fs::SeekMode;
using fs::SeekSet;
using fs::SeekCur;
using fs::SeekEnd;
using fs::FSInfo;
using fs::FSConfig;
using fs::SPIFFSConfig;
#endif //FS_NO_GLOBALS

#endif //FS_H
