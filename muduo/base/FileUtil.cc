// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/FileUtil.h"
#include "muduo/base/Logging.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

// fp_则为打开文件的指针。
FileUtil::AppendFile::AppendFile(StringArg filename)
  : fp_(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC，打开的文件。
    writtenBytes_(0)                            // 已经写入的字节数。
{
  assert(fp_);
  ::setbuffer(fp_, buffer_, sizeof buffer_);
  // posix_fadvise POSIX_FADV_DONTNEED ?
}

FileUtil::AppendFile::~AppendFile()
{ // 关键文件。
  ::fclose(fp_);
}

void FileUtil::AppendFile::append(const char* logline, const size_t len)
{
  size_t written = 0;

  while (written != len)
  {
    size_t remain = len - written;
    size_t n = write(logline + written, remain);
    if (n != remain)
    {
      int err = ferror(fp_);
      if (err)
      {
        fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
        break;
      }
    }
    written += n;
  }

  writtenBytes_ += written;
}

void FileUtil::AppendFile::flush()
{
  ::fflush(fp_);
}

size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
  // #undef fwrite_unlocked
  // 不加锁的方式添加，效率会更快。
  return ::fwrite_unlocked(logline, 1, len, fp_);
}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
  : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}

FileUtil::ReadSmallFile::~ReadSmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

// return errno
template<typename String>
int FileUtil::ReadSmallFile::readToString(int maxSize,
                                          String* content,
                                          int64_t* fileSize,
                                          int64_t* modifyTime,
                                          int64_t* createTime)
{
  static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
  assert(content != NULL);
  int err = err_;
  if (fd_ >= 0)
  {
    content->clear();

    if (fileSize)
    {
      struct stat statbuf;
      if (::fstat(fd_, &statbuf) == 0)
      {
        if (S_ISREG(statbuf.st_mode))
        {
          *fileSize = statbuf.st_size;
          content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        }
        else if (S_ISDIR(statbuf.st_mode))
        {
          err = EISDIR;
        }
        if (modifyTime)
        {
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime)
        {
          *createTime = statbuf.st_ctime;
        }
      }
      else
      {
        err = errno;
      }
    }

    while (content->size() < implicit_cast<size_t>(maxSize))
    {
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      ssize_t n = ::read(fd_, buf_, toRead);
      if (n > 0)
      {
        content->append(buf_, n);
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

int FileUtil::ReadSmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0); // 可以从偏移位置读。
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

