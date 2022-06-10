#pragma once

#include <cstring>
#include <string>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <utility>


namespace glplay::nix {

  class FileDescriptor {
    public:
      explicit FileDescriptor(std::string &path, int flags);
      FileDescriptor(const FileDescriptor& other); //Copy constructor
      FileDescriptor(FileDescriptor&& other) noexcept; //Move constructor
      auto operator=(const FileDescriptor& other) -> FileDescriptor&; //Copy assignment
      auto operator=(FileDescriptor&& other) noexcept -> FileDescriptor&; //Move assignment
      [[nodiscard]] auto fileDescriptor() const -> int { return fd; }
      [[nodiscard]] auto filePath() const -> std::string { return path; }

      ~FileDescriptor();
    private:
      std::string path;
      int flags = -1;
      int fd = -1;
  };

}
