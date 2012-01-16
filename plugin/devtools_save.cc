// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "devtools_save.h"

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#if defined(OS_WIN)
#include <io.h>
#endif
#include "logging.h"

#include "nputils.h"

template<typename T, size_t N> size_t ArraySize(T (&)[N]) {
  return N;
}

class SecurityChecks {
 private:
  static bool IsSlash(char c) {
#if defined(OS_POSIX)
    return c == '/';
#elif defined(OS_WIN)
    return c == '/' || c == '\\';
#else
#error FIXME: this is not implemented for your OS yet.
#endif
  }

 public:
  static bool IsAbsolutePath(const char* filename) {
#if defined(OS_POSIX)
    return filename[0] == '/';
#elif defined(OS_WIN)
	  return isalpha(filename[0]) && filename[1] == ':' && IsSlash(filename[2]);
#else
#error FIXME: this is not implemented for your OS yet.
#endif
  }

  static bool HasBackreferences(const char* filename) {
#if defined(OS_POSIX)
    return strstr("/../", filename) >= 0;
#elif defined(OS_WIN)
    for (; *filename; ++filename) {
	    if (IsSlash(*filename) &&
		      filename[1] == '.' && filename[2] == '.' &&
          IsSlash(filename[3]))
              return true;
	  }
	  return false;
#else
#error FIXME: this needs to be implemented yet.
#endif
  }

  static bool IsDevToolsSaveAllowed(const char* filename) {
    static const char allow_flag_file[] = ".allow-devtools-save";
    std::string path = filename;
    bool found = false;
    if (!path.length())
      return false;
    for (size_t pos = path.length() - 1; pos && !found; --pos) {
      if (!IsSlash(path[pos]))
        continue;
      path.replace(path.begin() + pos + 1, path.end(), allow_flag_file);
      struct stat sb;
      if (stat(path.c_str(), &sb) < 0)
        continue;
#if !defined(OS_WIN)
      if (!S_ISREG(sb.st_mode))
        continue;
#endif
      found = true;
    }
    return found;
  }
};

void DevToolsSave::Save(const char* filename, const char* content) {
  if (!SecurityChecks::IsAbsolutePath(filename)) {
    NPUtils::Throw(this, "Absolute path required!");
    return;
  }
  if (!SecurityChecks::IsDevToolsSaveAllowed(filename)) {
    NPUtils::Throw(this, "Missing .allow-devtools-save file in path from file to root!");
    return;
  }
  size_t size = strlen(content);
  DLOG(INFO) << "Saving to " << filename << " " << size << " bytes";
#if defined(OS_WIN)
  int fd = _open(filename, _O_WRONLY | _O_TRUNC);
#else
  int fd = open(filename, O_WRONLY | O_NOFOLLOW);
#endif
  if (fd < 0) {
    NPUtils::Throw(this, "Failed to open file. NOTE: the file needs to exist, we never create files!");
    return;    
  }

#if defined(OS_POSIX)
  bool allow_write = false;

  // Be paranoid -- only write to regular, non-executable files that we own.
  struct stat stat_info;

  if (fstat(fd, &stat_info) < 0)
    NPUtils::Throw(this, "Failed to stat the file!");
  else if (!S_ISREG(stat_info.st_mode))
    NPUtils::Throw(this, "Not a regular file!");
  else if (stat_info.st_uid != getuid())
    NPUtils::Throw(this, "Not an owner!");
  else if (stat_info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
    NPUtils::Throw(this, "Won't write to an executable file!");
  else
    allow_write = true;
#else
  const bool allow_write = true;
#endif

  if (allow_write) {
#if !defined(OS_WIN)
    ftruncate(fd, 0);
#endif

    do {
#if defined(OS_WIN)
      int written = _write(fd, content, size);
#else
      ssize_t written = write(fd, content, size);
#endif
      if (written < 0)
        break;
      size -= written;
      content += written;
    } while (size > 0);

    close(fd);

    if (size > 0)
      NPUtils::Throw(this, "Failed to write to file!");
    else
      DLOG(INFO) << "file written ok";
  }
}
