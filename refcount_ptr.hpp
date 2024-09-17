#pragma once

#include <atomic>

template <typename T>
class refcount_ptr {
  T *_ptr;
  std::atomic<int> *_count; // pointer for reference-count sharing
public:
  refcount_ptr() : _ptr(nullptr), _count(nullptr) {}
  refcount_ptr(T *ptr)
      : _ptr(ptr), _count(ptr ? new std::atomic<int>(1) : nullptr) {}
  refcount_ptr(const refcount_ptr &other)
      : _ptr(other._ptr), _count(other._count) {
    if (_count != nullptr) {
      _count->fetch_add(1);
    }
  }

  template <typename Y>
  refcount_ptr(Y *ptr)
      : _ptr(ptr), _count(ptr ? new std::atomic<int>(1) : nullptr) {}

  template <typename Y>
  refcount_ptr(const refcount_ptr<Y> &other, T *ptr) noexcept
      : _ptr(ptr), _count(other._count) // get_shared_count())
  {
    if (_count != nullptr) {
      _count->fetch_add(1);
    }
  }

  refcount_ptr(refcount_ptr &&other) noexcept
      : _ptr(other._ptr), _count(other._count) {
    other._ptr = nullptr;
    other._count = nullptr;
  }

  ~refcount_ptr() {
    if (_count != nullptr && _count->fetch_sub(1) == 1) {
      delete _ptr;
      delete _count;
    }
  }

  void reset(T *ptr) {
    if (_count != nullptr && _count->fetch_sub(1) == 1) {
      delete _ptr;
      delete _count;
    }
    _ptr = ptr;
    _count = ptr ? new std::atomic<int>(1) : nullptr;
  }

  refcount_ptr &operator=(const refcount_ptr &other) {
    if (this != &other) {
      if (_count != nullptr && _count->fetch_sub(1) == 1) {
        delete _ptr;
        delete _count;
      }

      _ptr = other._ptr;
      _count = other._count;
      if (_count != nullptr) {
        _count->fetch_add(1);
      }
    }
    return *this;
  }

  refcount_ptr &operator=(refcount_ptr &&other) noexcept {
    if (this != &other) {
      if (_count != nullptr && _count->fetch_sub(1) == 1) {
        delete _ptr;
        delete _count;
      }
      _ptr = other._ptr;
      _count = other._count;
      other._ptr = nullptr;
      other._count = nullptr;
    }
    return *this;
  }

  T *get() const { return _ptr; }
  bool operator!() const { return !_ptr; }
  T &operator*() const { return *_ptr; }
  T *operator->() const { return _ptr; }
};

template <typename T, typename... Args>
refcount_ptr<T> make_refcount_ptr(Args &&...args) {
  return refcount_ptr<T>(new T(std::forward<Args>(args)...));
}
