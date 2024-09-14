#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <thread>
#include <unordered_set>
#include <vector>
#include <ctime>
#include <iomanip>
#include <iostream>

using namespace std::literals;

#define DEBUG_METHOD()                                                         \
  std::cout << timestamp() << " " << __PRETTY_FUNCTION__ << " @ " << this << std::endl
#define DEBUG_VALUE_OF(x) std::cout << timestamp() << " " << #x << "=" << x << std::endl
#define DEBUG_MESSAGE(x) std::cout << timestamp() << " " << x << std::endl

std::string timestamp() {
    // get a precise timestamp as a string
  const auto now = std::chrono::system_clock::now();
  const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
  const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;
  std::stringstream ss;
  ss
      << std::put_time(std::localtime(&nowAsTimeT), "%a %b %d %Y %T")
      << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
  return ss.str();
}

template <typename F> void call_async(F &&fun) {
  auto futptr = std::make_shared<std::future<
      void>>(); // passing this by value to the lambda will increment the
                // counter and ensure completion, even after going oos
  *futptr = std::async(std::launch::async, [futptr, fun]() { fun(); });
}

namespace rx {
struct on_complete : public std::exception {};

template <typename T> using observer = std::function<void(const T &)>;

template <typename T> class observable {
public:
  using observer_t = observer<T>;
  using completer_t = std::function<void()>;
  using subscribe_callback = std::function<void(const observer_t &)>;

private:
  subscribe_callback _subscribe_callback;

  std::vector<completer_t> _completers;

  void subscribe_impl(const observer_t &obj) {
    try {
      _subscribe_callback(obj);
    } catch (on_complete &oc) {
      return;
    }
  }
  void subscribe_impl(const completer_t &obj) { _completers.push_back(obj); }

public:
  observable(subscribe_callback fun) : _subscribe_callback(fun) {}

  observable(const observable &other)
      : _subscribe_callback(other._subscribe_callback) {}
  observable operator=(const observable &) const = delete;

  virtual ~observable() {
    // DEBUG_METHOD();
  }

  template <typename... Ts> void subscribe(Ts &&...ts) {
    (subscribe_impl(std::forward<Ts>(ts)), ...);

    for (auto complete : _completers) {
      complete();
    }
  }
  template <typename Pred> auto filter(Pred &&pred) {
    return make_shared_observable<T>([this, pred](const observer_t &obs) {
      this->subscribe([pred, obs](const T &t) {
        if (pred(t)) {
          obs(t);
        }
      });
    });
  }

  template <typename Period> auto delay(const Period &a_while) {
    return make_shared_observable<T>([=](const observer_t &obs) {
      this->subscribe([=](const T &t) {
        std::this_thread::sleep_for(a_while);
        obs(t);
      });
    });
  }

  template <typename Period> auto debounce(const Period &timeout) {
    using clock_t = std::chrono::steady_clock;
    return make_shared_observable<T>([this, timeout](const observer_t &obs) {
      auto last_time = clock_t::now();
      bool is_first = true;
      this->subscribe([=, &last_time, &is_first](const T &value) {
        // when a new value comes in, check if the previous value arrived before
        // the `timeout`
        // if it didn't -> emit new value
        auto current_time = clock_t::now();
        if (current_time - last_time < timeout) {
          obs(value);
          // is_first = false;
        }
        last_time = current_time;
      });
    });
  }

  template <typename F> auto map(F &&fun) {
    return make_shared_observable<T>([this, fun](const observer_t &obs) {
      this->subscribe([=](const T &t) { obs(fun(t)); });
    });
  }

  template <typename F> auto reduce(F &&fun, T seed = T{0}) {
    return make_shared_observable<T>([this, fun, seed](const observer_t &obs) {
      T result = seed;
      this->subscribe(
          // next
          [=, &result](const T &t) { result = fun(result, t); },
          // completed
          [&] { obs(result); });
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
      this->subscribe([next, &last](const T &value) { last = value; },
                      [next, &last] { next(last); });
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
      this->subscribe([this, &count, obs, n](const T &value) {
        if (count++ < n) {
          obs(value);
        } else {
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
  using Mapper = std::function<std::shared_ptr<observable<U>>(const T &)>;

  template <typename U> auto flat_map(Mapper<U> &&mapper) {
#if 0
    return make_shared_observable<U>(
        [this, mapper](std::function<void(const U &)> on_next) {
          std::stack<std::shared_ptr<observable<U>>> innerObservables;
          std::stack<observer<U>> innerObservers;

          this->subscribe([mapper, &innerObservables, &innerObservers,
                           on_next](const T &value) {
            innerObservables.push(mapper(value));
            innerObservers.push(on_next);

            while (!innerObservables.empty()) {
              innerObservables.top()->subscribe(innerObservers.top());
              innerObservables.pop();
              innerObservers.pop();
            }
          });
        });
#else
    return make_shared_observable<U>([this, mapper](observer<U> next) {
      this->subscribe([mapper, next](const T &value) {
        return mapper(value)->subscribe(next);
      });
    });
#endif
  }

  template <typename Period> auto sample(Period period) {
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

  template <typename Predicate> auto skip_while(Predicate predicate) {
    return make_shared_observable<T>(
        [this, predicate](const observer_t &on_next) {
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
    return make_shared_observable<bool>([this, predicate](const observer_t& on_next) {
        bool ret = true;
        this->subscribe(
            [predicate, on_next, &ret](const T& value) {
                if (!predicate(value)) {
                    //on_next(false);
                    ret = false;
                    throw on_complete();
                }
            },
            [on_next, &ret]() {
                on_next(ret);
            }
        );
    });
}


  template <typename U, typename... Ts>
  static auto make_shared_observable(Ts &&...ts) {
    return std::make_shared<observable<U>>(std::forward<Ts>(ts)...);
  }
};

template <typename T>
static auto defer(std::function<observable<T>()> factory) {
  return observable<T>::template make_shared_observable<T>([factory](const observer<T>& on_next) {
        factory().subscribe(on_next);
    });
}

template <typename T, typename Period>
static auto interval(const Period &a_while) {
  using clock_t = std::chrono::steady_clock;
  return observable<T>::template make_shared_observable<T>(
      [=](const observer<T> &next) {
        T count = T{0};
        while (true) {
          next(count++);
          std::this_thread::sleep_for(a_while);
        }
      });
}
template <typename T> static auto repeat(T value, size_t count) {
  return observable<T>::template make_shared_observable<T>(
      [=](const observer<T> &next) {
        for (size_t i = 0; i < count; i++) {
          next(value);
        }
      });
}

template <typename Iterable> auto from(Iterable iterable) {
  using T = typename std::remove_reference<decltype(*iterable.begin())>::type;
  return observable<T>::template make_shared_observable<T>(
      [iterable](const typename observable<T>::observer_t next) {
        for (auto i : iterable) {
          next(i);
        }
      });
}

template <typename... Ts> auto of(Ts &&...ts) {
  using T = typename std::common_type<Ts...>::type;
  return observable<T>::template make_shared_observable<T>(
      [ts...](const typename observable<T>::observer_t next) {
        std::initializer_list<T> list{(ts)...};
        for (auto i : list) {
          next(i);
        }
      });
}
template <typename T> static auto range(T start, T count) {
  return observable<T>::template make_shared_observable<T>(
      [start, count](const observer<T> &obs) {
        for (T i = start; i < start + count; ++i) {
          obs(i);
        }
      });
}

template <typename Fun>
auto start(Fun&& factory) {
    using T = typename std::invoke_result<Fun>::type;
    return observable<T>::template make_shared_observable<T>([factory](const observer<T>& on_next) {
        on_next(factory());
    });
}


} // namespace rx
  //

int foo() { return 42; }

int main() {
  std::atomic<bool> is_done = false;

    rx::start(foo)->subscribe([] (int i) { DEBUG_VALUE_OF(i); });
    rx::of(1, 2, 3, 4, 5, 6, 7)
      ->delay(1ms)
      ->all([] (int value) { return value > 0; })
      ->subscribe([](int i) { 
            DEBUG_VALUE_OF(i); 
        },
        [] { 
            std::cout << "done!" << std::endl; 
        });

  rx::range(1, 10)
      ->flat_map<int>([](auto val) {
        return rx::of(val, 3)->delay(10ms)->first()->map(
            [](auto x) { return x * x; });
      })
      ->delay(300ms)
      ->sample(500ms)
      ->subscribe(
          [](int value) {
            DEBUG_VALUE_OF(value);
          },
          [&is_done] {
            DEBUG_VALUE_OF("Sequence complete!");
            is_done = true;
          });

  std::map<int, std::chrono::milliseconds> times = {
      {0, 100ms}, {1, 600ms}, {2, 400ms}, {3, 700ms}, {4, 200ms}};
  rx::from(times)
      ->flat_map<int>(
          [](auto p) { return rx::of(p.first)->delay(p.second); }) // 0, 2, 4
      ->debounce(500ms)
      ->subscribe([](auto i) { DEBUG_VALUE_OF(i); });

  return 0;
}
