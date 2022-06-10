#include "FileDescriptor.hpp"

namespace glplay::nix {

  //Constructor
  FileDescriptor::FileDescriptor(std::string &path, int flags): path(path), flags(flags), fd(open(path.c_str(), O_RDWR | O_CLOEXEC, 0)) {
    if(fd == -1) {
      throw std::runtime_error("Error opening file descriptor: " + path + ": " + strerror(errno));
    }
  }

  //Destructor
  FileDescriptor::~FileDescriptor() {
    if (fd != -1) {
      close(fd);
    }
  }

  //Copy constructor
  // Do deep copy
  FileDescriptor::FileDescriptor(const FileDescriptor& other) : path(other.path), flags(other.flags), fd(dup(other.fd)) { 
    if (fd < 0) {
      throw std::runtime_error(strerror(errno));
    }
  }

  // Move constructor
  // Transfer ownership 
  FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept : path(other.path), flags(other.flags), fd(other.fd) { 
    other.fd = -1;
    other.flags = -1;
    other.path = "";
  }

  // Copy assignment
  // Do deep copy 
  auto FileDescriptor::operator=(const FileDescriptor& other) -> FileDescriptor& {
    if (&other == this) {
      return *this;
    }
    if(fd >= 0) {
      close(fd);
    }
    fd = dup(other.fd);
    path = other.path;
    flags = other.flags;
    return *this;
  }

  // Move assignment
  // Transfer ownership 
  auto FileDescriptor::operator=(FileDescriptor&& other) noexcept -> FileDescriptor& {
    if (&other == this) {
      return *this;
    }
    if(fd >= 0) {
      close(fd);
    }
    fd = dup(other.fd);
    path = other.path;
    flags = other.flags;
    other.fd = -1;
    other.flags = -1;
    other.path = "";
    return *this;
  }

}