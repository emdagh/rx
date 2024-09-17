#pragma once

#include <atomic>

template <typename T>
class refcount_ptr {
  T *_m;
  std::atomic<int> *_count; // pointer for reference-count sharing
public:
  refcount_ptr() : _m(nullptr) {}
  refcount_ptr(T *m) : _m(m), _count(m ? new std::atomic<int>(1) : nullptr) {}
  refcount_ptr(const refcount_ptr &other) : _m(other._m), _count(other._count) {
    if (_count) {
      _count->fetch_add(1);
    }
  }

  template <typename Y>
  refcount_ptr(Y *ptr)
      : _m(ptr), _count(ptr ? new std::atomic<int>(1) : nullptr) {}

  template <typename Y>
  refcount_ptr(const refcount_ptr<Y> &other, T *ptr) noexcept
      : _m(ptr), _count(other._count) // get_shared_count())
  {
    if (_count) {
      _count->fetch_add(1);
    }
  }

  refcount_ptr(refcount_ptr &&other) noexcept
      : _m(other._m), _count(other._count) {
    other._m = nullptr;
    other._count = nullptr;
  }

  ~refcount_ptr() {
    if (_count && _count->fetch_sub(1) == 1) {
      delete _m;
      delete _count;
    }
  }

  void reset(T *p) {
    if (_count && _count->fetch_sub(1) == 1) {
      delete _m;
      delete _count;
    }
    _m = p;
    _count = p ? new std::atomic<int>(1) : nullptr;
  }

  refcount_ptr &operator=(const refcount_ptr &other) {
    if (this != &other) {
      if (_count && _count->fetch_sub(1) == 1) {
        delete _m;
        delete _count;
      }

      _m = other._m;
      _count = other._count;
      if (_count)
        _count->fetch_add(1);
    }
    return *this;
  }

  refcount_ptr &operator=(refcount_ptr &&other) noexcept {
    if (this != &other) {
      if (_count && _count->fetch_sub(1) == 1) {
        delete _m;
        delete _count;
      }
      _m = other._m;
      _count = other._count;
      other._m = nullptr;
      other._count = nullptr;
    }
    return *this;
  }

  T *get() const { return _m; }
  bool operator!() const { return !_m; }
  T &operator*() const { return *_m; }
  T *operator->() const { return _m; }
};

template <typename T, typename... Args>
refcount_ptr<T> make_refcount_ptr(Args &&...args) {
  return refcount_ptr<T>(new T(std::forward<Args>(args)...));
}