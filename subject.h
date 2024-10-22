#pragma once

#include "rx.hpp"
#include <cstdint>
#include <list>
#include <vector>

template <typename T>
class subject : public rx::observable<T> {
    std::vector<rx::observer<T>> _observables;

  public:
    subject()
        : rx::observable<T>([this](const rx::observer<T> &obs) {
            _observables.push_back(obs);
        }) {}

    void on_next(const T &t) {
        for (const auto &next : _observables) {
            next(t);
        }
    }

    void on_completed() {}

    void on_error(const std::exception_ptr &ep) {}
};

template <typename T>
class behavior_subject : public rx::observable<T> {
    T _current;
    std::vector<rx::observer<T>> _lst;

  public:
    explicit behavior_subject(const T &t)
        : _current(t)
        , rx::observable<T>([this](const rx::observer<T> &obs) {
            obs(_current);
            _lst.push_back(obs);
        }) {}
    virtual ~behavior_subject() {}
    virtual void on_next(const T &t) {
        _current = t;
        for (auto &next : _lst) {
            next(t);
        }
    }
};

template <typename T>
class replay_subject : public rx::observable<T> {
    size_t _len;
    std::list<T> _q;
    std::vector<rx::observer<T>> _lst;

  public:
    replay_subject(size_t buf_len)
        : rx::observable<T>([this](const rx::observer<T> &obs) {
            for (auto &item : _q) {
                obs(item);
            }
            _lst.push_back(obs);
        })
        , _len(buf_len) {}
    virtual ~replay_subject() {}

    virtual void on_next(const T &t) {
        _q.push_back(t);
        if (_q.size() > _len) {
            _q.pop_front();
        }
        for (auto &next : _lst) {
            next(t);
        }
    }
};
