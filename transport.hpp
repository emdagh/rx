#pragma once

#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

template <typename T, typename CharT = std::char_traits<T>>
class basic_instreambuf : public std::basic_streambuf<T, CharT> {
public:
  basic_instreambuf(int socket_fd) : socket_fd_(socket_fd), buffer_size_(4096) {
    buffer_ = std::make_unique<char[]>(buffer_size_);
  }

protected:
  virtual int underflow() {
    if (this->getpos() >= this->egptr()) {
      int bytes_read = read(socket_fd_, buffer_.get(), buffer_size_);
      if (bytes_read <= 0) {
        return EOF;
      }
      this->setg(buffer_.get(), buffer_.get(), buffer_.get() + bytes_read);
    }
    return *this->gptr();
  }

  virtual int overflow(int c = EOF) {
    if (c != EOF) {
      if (this->pptr() >= this->epptr()) {
        // Buffer is full, try to flush it first
        int bytes_written =
            write(socket_fd_, this->pbase(), this->pptr() - this->pbase());
        if (bytes_written < 0) {
          return EOF;
        }
      }
      *this->pptr() = c;
      this->pbump(1);
    }
    return c;
  }

private:
  int socket_fd_;
  std::unique_ptr<char[]> buffer_;
  size_t buffer_size_;
};

template <typename T, typename CharT = std::char_traits<T>>
class basic_instream : public std::basic_istream<T, CharT> {
public:
  basic_instream(int socket_fd) : std::istream(streambuf_.get()) {
    streambuf_.setbuf(buffer_.get(), buffer_size_);
  }

private:
  std::unique_ptr<basic_instreambuf<T, CharT>> streambuf_;
  std::unique_ptr<char[]> buffer_;
  size_t buffer_size_;
};

template <typename T, typename CharT = std::char_traits<T>>
class basic_onstream : public std::basic_ostream<T, CharT> {
public:
  basic_onstream(int socket_fd)
      : streambuf_(std::make_unique<basic_instreambuf<T, CharT>>(socket_fd)),
        std::ostream(streambuf_.get()) {
    streambuf_->setbuf(buffer_.get(), buffer_size_);
  }

private:
  std::unique_ptr<basic_instreambuf<T, CharT>> streambuf_;
  std::unique_ptr<char[]> buffer_;
  size_t buffer_size_;
};