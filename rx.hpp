#pragma once

#include "refcount_ptr.hpp"
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <thread>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#define DEBUG_METHOD() std::cout << timestamp() << " " << __PRETTY_FUNCTION__ << " @ " << this << std::endl
#define DEBUG_VALUE_OF(x) std::cout << timestamp() << " " << #x << "=" << (((x))) << std::endl
#define DEBUG_MESSAGE(x) std::cout << timestamp() << " " << (((x))) << std::endl
#define DEBUG_VALUE_AND_TYPE_OF(x)                                                                                     \
    std::cout << timestamp() << " " << #x << "=" << (((x))) << " [" << typeid((((x)))).name() << "]" << std::endl
template <typename T, typename U>
std::ostream &operator<<(std::ostream &os, const std::pair<T, U> &v) {
    os << v.first << "=" << v.second;
    return os;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
    std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ","));
    return os;
}

template <typename Rep, typename Period>
std::ostream &operator<<(std::ostream &os, const std::chrono::duration<Rep, Period> &duration) {
    os << duration.count();
    return os;
}
template <typename T>
std::ostream &operator<<(std::ostream &os, const std::optional<T> &v) {
    if (v.has_value()) {
        os << v.value();
    } else {
        os << "null";
    }
    return os;
}

std::string timestamp() {
    // get a precise timestamp as a string
    const auto now = std::chrono::system_clock::now();
    const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream sss;
    sss << std::put_time(std::localtime(&nowAsTimeT), "%a %b %d %Y %T") << '.' << std::setfill('0') << std::setw(3)
        << nowMs.count();
    return sss.str();
}
namespace rx {

template <typename Iterable>
auto from(Iterable iterable);
template <typename... Ts>
auto of(Ts &&...ts);

struct on_complete : public std::exception {};

template <typename T>
using observer = std::function<void(const T &)>;

template <typename T>
class observable;

template <typename T>
using shared_observable = refcount_ptr<observable<T>>;

template <typename T>
class observable {
  public:
    using observer_t = observer<T>;
    using completer_t = std::function<void()>;
    using subscribe_callback = std::function<void(const observer_t &)>;

  private:
    bool _is_completed = false;
    subscribe_callback _subscribe_callback;

    std::vector<completer_t> _completers;

    void subscribe_impl(const observer_t &obj) {
        try {
            _subscribe_callback(obj);
        } catch (on_complete &oc) {
            this->_is_completed = true;
            return;
        }
    }
    void subscribe_impl(const completer_t &obj) { _completers.push_back(obj); }

  public:
    observable(subscribe_callback fun)
        : _subscribe_callback(fun) {}

    observable(const observable &other)
        : _subscribe_callback(other._subscribe_callback)
        , _completers(other._completers)
        , _is_completed(other._is_completed) {}

    virtual ~observable() {
        // DEBUG_METHOD();
    }

    observable &operator=(const observable &other) const {
        observable copy(other);
        std::swap(copy, *this);
        return *this;
    }

    bool is_completed() const { return _is_completed; }

    template <typename... Ts>
    void subscribe(Ts &&...ts) {
        (subscribe_impl(std::forward<Ts>(ts)), ...);

        for (const auto &complete : _completers) {
            complete();
        }
    }
    template <typename Pred>
    auto filter(Pred &&pred) {
        return make_shared_observable<T>([this, pred](const observer_t &obs) {
            this->subscribe([pred, obs](const T &t) {
                if (pred(t)) {
                    obs(t);
                }
            });
        });
    }

    template <typename Period>
    auto delay(const Period &a_while) {

        return make_shared_observable<T>([=](const observer_t &obs) {
            std::this_thread::sleep_for(a_while);
            this->subscribe([=](const T &t) {
                obs(t);
            });
        });
    }

    template <typename Period>
    auto debounce(const Period &timeout) {
        using clock_t = std::chrono::steady_clock;
        return make_shared_observable<T>([this, timeout](const observer_t &obs) {
            auto last_time = clock_t::now();
            bool is_first = true;
            this->subscribe([=, &last_time, &is_first](const T &value) {
                // when a new value comes in, check if the previous value
                // arrived before the `timeout` if it didn't -> emit new
                // value
                auto current_time = clock_t::now();
                if (current_time - last_time < timeout) {
                    obs(value);
                }
                last_time = current_time;
            });
        });
    }

    template <typename F>
    auto map(F &&fun) {
        return make_shared_observable<T>([this, fun](const observer_t &obs) {
            this->subscribe([=](const T &t) {
                obs(fun(t));
            });
        });
    }

    template <typename U>
    auto scan(U s, std::function<U(U, const T &)> accumulator) {
        return make_shared_observable<U>([this, s, accumulator](std::function<void(const U &)> on_next) {
            U seed = s;
            this->subscribe(
                [&seed, accumulator, on_next](const T &value) {
                    seed = accumulator(seed, value);
                    on_next(seed);
                },
                [seed, on_next]() {
                    on_next(seed);
                });
        });
    }

    template <typename Duration>
    auto time_interval() {
        return make_shared_observable<Duration>([this](std::function<void(const Duration &)> on_next) {
            using clock_t = std::chrono::steady_clock;
            auto lastTime = clock_t::now();
            this->subscribe([on_next, &lastTime](const T &value) {
                auto currentTime = clock_t::now();
                on_next(std::chrono::duration_cast<Duration>(currentTime - lastTime));
                lastTime = currentTime;
            });
        });
    }

    template <typename F>
    auto reduce(F &&fun, T seed = T{0}) {
        return make_shared_observable<T>([this, fun, seed](const observer_t &obs) {
            T result = seed;
            this->subscribe(
                // next
                [=, &result](const T &t) {
                    result = fun(result, t);
                },
                // completed
                [&] {
                    obs(result);
                });
        });
    }

    auto distinct() {
        return make_shared_observable<T>([this](const observer_t &next) {
            std::unordered_set<T> seen = {};
            this->subscribe([&](const T &value) {
                if (seen.insert(value).second) {
                    next(value);
                }
            });
        });
    }

    auto last() {
        return make_shared_observable<T>([this](const observer_t &next) {
            T last;
            this->subscribe(
                [next, &last](const T &value) {
                    last = value;
                },
                [next, &last] {
                    next(last);
                });
        });
    }

    auto skip(size_t n) {
        return make_shared_observable<T>([this, n](const observer_t &next) {
            size_t count = 0;
            this->subscribe([&count, next, n](const T &value) {
                if (count++ >= n) {
                    next(value);
                }
            });
        });
    }
    auto take(size_t n) {
        return make_shared_observable<T>([this, n](const observer_t &obs) {
            size_t count = 0;
            bool has_completed = false;
            this->subscribe([this, &count, obs, n, &has_completed](const T &value) {
                obs(value);
                if (++count >= n) {
                    has_completed = true;
                    throw on_complete();
                }
            });
        });
    }
    auto average() {
        return make_shared_observable<T>([this](const observer_t &obs) {
            float sum = 0.0f;
            size_t n = 0;
            this->subscribe([this, obs, &sum, &n](const T &value) {
                sum += static_cast<decltype(sum)>(value);
                obs(sum / ++n);
            });
        });
    }
    auto first() {
        return make_shared_observable<T>([this](const observer_t &next) {
            bool is_first = true;
            this->subscribe([&is_first, next](const T &value) {
                if (is_first) {
                    next(value);
                    is_first = false;
                }
            });
        });
    }

    template <typename U>
    using Mapper = std::function<refcount_ptr<observable<U>>(const T &)>;

    template <typename U>
    auto flat_map(Mapper<U> mapper) {
        return make_shared_observable<U>([this, mapper](observer<U> next) {
            this->subscribe([mapper, next](const T &value) {
                return mapper(value)->subscribe(next);
            });
        });
    }

    template <typename Period>
    auto buffer_with_time(const Period &period) {
        using U = std::vector<T>;
        using clock_t = std::chrono::steady_clock;

        return make_shared_observable<U>([this, period](observer<U> on_next) {
            U buffer = {};

            auto when = clock_t::now() + period;

            this->subscribe(
                [&buffer, &when, period, on_next](const T &val) {
                    buffer.push_back(val);
                    auto now = clock_t::now();
                    if (now >= when) {
                        on_next(buffer);
                        buffer.clear();
                        when = now + period;
                    }
                },
                [on_next, &buffer] {
                    // clear out any remainders
                    if (buffer.size() > 0) {
                        on_next(buffer);
                        buffer.clear();
                    }
                });
        });
    }

    auto buffer_with_count(size_t n) {
        using U = std::vector<T>;

        return make_shared_observable<U>([this, n](observer<U> on_next) {
            U buffer = {};

            this->subscribe(
                [&buffer, n, on_next](const T &val) {
                    buffer.push_back(val);
                    if (buffer.size() >= n) {
                        on_next(buffer);
                        buffer.clear();
                    }
                },
                [on_next, &buffer] {
                    // clear out any remainders
                    if (!buffer.empty()) {
                        on_next(buffer);
                        buffer.clear();
                    }
                });
        });
    }

    template <typename Duration>
    auto window(const Duration &duration) {
        using U = refcount_ptr<observable<T>>; // std::vector<T>;
        using clock_t = std::chrono::steady_clock;

        return make_shared_observable<U>([this, duration](observer<U> on_next) {
            std::vector<T> buffer = {};

            auto when = clock_t::now() + duration;

            this->subscribe(
                [on_next, &buffer, &when, duration](const T &val) {
                    buffer.push_back(val);
                    auto now = clock_t::now();
                    if (now >= when) {
                        on_next(rx::from(buffer));
                        buffer.clear();
                        when = now + duration;
                    }
                },
                [on_next, &buffer] {
                    // clear out any remainders
                    if (buffer.size() > 0)
                        on_next(rx::from(buffer));
                });
        });
    }

    template <typename KeySelector> //, typename ValueSelector>
    auto group_by(KeySelector key_for) {
        using U = std::vector<T>;
        using Y = refcount_ptr<observable<T>>; // std::optional<std::pair<T,
                                               // U>>;
        return make_shared_observable<Y>([this, key_for](observer<Y> on_next) {
            std::unordered_map<T, U> buffer = {};

            this->subscribe(
                [on_next, &buffer, key_for](const T &value) {
                    buffer[key_for(value)].push_back(value);
                },
                [on_next, &buffer] {
                    for (auto group : buffer) {
                        on_next(rx::from(group.second));
                    }
                });
        });
    }

    template <typename U>
    auto if_then_else(std::function<bool(const T &)> predicate, const refcount_ptr<observable<U>> &then_,
                      const refcount_ptr<observable<U>> &else_) {
        return make_shared_observable<U>([this, predicate, then_, else_](observer<U> on_next) {
            this->subscribe([predicate, then_, else_, on_next](const T &value) {
                if (predicate(value)) {
                    then_->subscribe(on_next);
                } else {
                    else_->subscribe(on_next);
                }
            });
        });
    }

    template <typename Period>
    auto sample(Period period) {
        using clock_t = std::chrono::steady_clock;

        return make_shared_observable<T>([this, period](const observer_t &obs) {
            auto timer = clock_t::now() + period;
            this->subscribe([&timer, period, obs](const T &value) {
                if (clock_t::now() >= timer) {
                    obs(value);
                    timer += period;
                }
            });
        });
    }

    template <typename Predicate>
    auto skip_while(Predicate predicate) {
        return make_shared_observable<T>([this, predicate](const observer_t &on_next) {
            bool is_skipping = true;
            this->subscribe([predicate, on_next, &is_skipping](const T &value) {
                if (is_skipping && predicate(value)) {
                    return;
                }
                is_skipping = false;
                on_next(value);
            });
        });
    }

    template <typename Predicate>
    auto all(Predicate predicate) {
        return make_shared_observable<bool>([this, predicate](const observer_t &on_next) {
            bool ret = true;
            this->subscribe(
                [predicate, on_next, &ret](const T &value) {
                    if (!predicate(value)) {
                        ret = false;
                        throw on_complete();
                    }
                },
                [on_next, &ret]() {
                    on_next(ret);
                });
        });
    }

    template <typename U = size_t>
    auto count() {
        return make_shared_observable<size_t>([this](const observer<U> &on_next) {
            U count = 0;
            this->subscribe(
                [&count](const T &t) {
                    count++;
                },
                [on_next, &count] {
                    on_next(count);
                });
        });
    }

    template <typename U>
    auto to(std::function<U(const T &)> mapper) {
        return make_shared_observable<U>([this, mapper](std::function<void(const U &)> on_next) {
            this->subscribe([mapper, on_next](const T &value) {
                on_next(mapper(value));
            });
        });
    }

    template <typename Container>
    auto to_iterable() {
        return make_shared_observable<Container>([this](observer<Container> on_next) {
            Container res = {};
            auto o_first = std::back_inserter(res);

            this->subscribe(
                [&o_first](const T &t) {
                    *o_first++ = t;
                },
                [on_next, &res] {
                    on_next(res);
                });
        });
    }

    template <typename U, typename... Ts>
    static auto make_shared_observable(Ts &&...args) {
        return make_refcount_ptr<observable<U>>(std::forward<Ts>(args)...);
    }
}; // observable

// helper
template <typename T, typename... Args>
static auto make_shared_observable(Args &&...args) {
    return make_refcount_ptr<observable<T>>(std::forward<Args>(args)...);
    // return observable<T>::template make_shared_observable<T>(
    //     std::forward<Args>(args)...);
}

template <typename T>
static auto defer(std::function<observable<T>()> factory) {
    return make_shared_observable<T>([factory](const observer<T> &on_next) {
        factory().subscribe(on_next);
    });
}

template <typename T, typename Period>
static auto interval(const Period &a_while) {
    using clock_t = std::chrono::steady_clock;
    return make_shared_observable<T>([=](const observer<T> &next) {
        T count = T{0};
        while (true) {
            next(count++);
            std::this_thread::sleep_for(a_while);
        }
    });
}
template <typename T>
static auto repeat(T value, size_t count) {
    return make_shared_observable<T>([=](const observer<T> &next) {
        for (size_t i = 0; i < count; i++) {
            next(value);
        }
    });
}

template <typename Iterable>
auto from(Iterable iterable) {
    using T = typename std::remove_reference<decltype(*iterable.begin())>::type;
    return make_shared_observable<T>([iterable](const typename observable<T>::observer_t next) {
        for (auto i : iterable) {
            next(i);
        }
    });
}

template <typename... Ts>
auto of(Ts &&...ts) {
    using T = typename std::common_type<Ts...>::type;
    return make_shared_observable<T>([ts...](observer<T> next) {
        std::initializer_list<T> list{(ts)...};
        for (auto i : list) {
            next(i);
        }
    });
}
template <typename T>
static auto range(T start, T count) {
    return make_shared_observable<T>([start, count](const observer<T> &obs) {
        for (T i = start; i < start + count; ++i) {
            obs(i);
        }
    });
}

template <typename Fun>
auto start(Fun &&factory) {
    using T = typename std::invoke_result<Fun>::type;
    return make_shared_observable<T>([factory](const observer<T> &on_next) {
        on_next(factory());
    });
}

template <typename T, typename Traits = std::char_traits<T>>
static auto from_istream(std::basic_istream<T, Traits> &iss) {
    using char_type = typename std::basic_istream<T, Traits>::char_type;
    return make_shared_observable<char_type>([&iss](const observer<char_type> &on_next) {
        while (!iss.eof() && !iss.bad()) {
            T value;
            iss >> value;
            if (iss.fail()) {
                break;
            }
            on_next(value);
        }
        throw on_complete();
    });
}

} // namespace rx
