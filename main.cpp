#include <list>
#include "refc_ptr.hpp"
#include "rx.hpp"
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

using namespace std::literals;

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
        }) {
        }
    virtual ~behavior_subject() {}
    virtual void on_next(const T& t) {
        _current = t;
        for(auto& next : _lst) {
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
        : rx::observable<T>([this] (const rx::observer<T>& obs) {
            for(auto& item : _q) {
                obs(item);
            }
            _lst.push_back(obs);
        })
        , _len(buf_len) {}
    virtual ~replay_subject() {}

    virtual void on_next(const T& t) {
        _q.push_back(t);
        if(_q.size() > _len) {
            _q.pop_front();
        }
        for(auto& next : _lst) {
            next(t);
        }
    }
};

template <typename F>
void call_async(F &&fun) {
    auto futptr = std::make_shared<std::future<void>>(); // passing this by value to the lambda will increment the
                                                         // counter and ensure completion, even after going oos
    *futptr = std::async(std::launch::async, [futptr, fun]() {
        fun();
    });
}

int foo() { return 42; }

template <typename T>
std::string lexical_cast(const T &t) {
    std::stringstream sss;
    sss << t;
    return sss.str();
}

#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

struct tcp_accept {
    int _fd;
    int _self;
    tcp_accept(int fd)
        : _fd(fd) {
        _self = accept(_fd, nullptr, nullptr);
        if (_self < 0)
            perror("accept");
    }

    ~tcp_accept() { close(_self); }
};

auto tcp_listener(uint16_t port) {
    return rx::make_observable<std::string>([port](const rx::observer<std::string> &on_next) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        int option = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
            perror("accept");
        listen(sock, 5);
        while (true) {
            tcp_accept client(sock);
            std::string data;
            while (true) {
                char buf[1024];
                int len = read(client._self, buf, sizeof(buf));
                if (len <= 0) {
                    on_next("client disconnected");
                    break;
                }
                on_next(std::string(buf, len));
            }
        }
        throw rx::on_complete();
        close(sock);
    });
}

#include <fstream>

int main() {

    subject<int> sub;
    sub.subscribe(
        [](int x) {
            DEBUG_VALUE_AND_TYPE_OF(x);
        },
        [](int y) {
            DEBUG_VALUE_AND_TYPE_OF(y);
        });
    sub.on_next(0);
    sub.on_next(1);
    sub.on_next(2);

    behavior_subject<int> bs(1337);
    bs.subscribe([] (int z) {
            DEBUG_VALUE_AND_TYPE_OF(z);
            });
    bs.on_next(1338);
    bs.subscribe([] (int a) {
        DEBUG_VALUE_AND_TYPE_OF(a);
    });

    replay_subject<int> rs(3);
    rs.subscribe([] (int n) {
            DEBUG_VALUE_AND_TYPE_OF(n);
            });

    rs.on_next(1);
    rs.on_next(2);
    rs.on_next(3);

    rs.subscribe([] (int m) {
            DEBUG_VALUE_AND_TYPE_OF(m);
            });

    rs.on_next(4);
    rs.subscribe([] (int o) {
            DEBUG_VALUE_AND_TYPE_OF(o);
            });


    exit(-2);

    // tcp_listener(5557)->take(1)->subscribe(
    //     [](const std::string &str) { DEBUG_VALUE_OF(str); });

    std::ifstream ifs;
    ifs.open("test.txt");
    rx::from_istream<char>(ifs)->to_iterable<std::string>()->subscribe([](auto c) {
        DEBUG_VALUE_AND_TYPE_OF(c);
    });
    DEBUG_MESSAGE("-buffer with time------------");
    rx::of(1, 2, 3, 4, 5, 6)
        ->flat_map<int>([](auto i) {
            return rx::of(i)->delay(100ms);
        })
        ->buffer_with_time(250ms)
        ->subscribe([](auto value) {
            DEBUG_VALUE_AND_TYPE_OF(value);
        });
    DEBUG_MESSAGE("-window----------------------");
    rx::of(1, 2, 3, 4, 5, 6)
        ->flat_map<int>([](auto i) {
            return rx::of(i)->delay(100ms);
        })
        ->window(250ms)
        ->subscribe([](const auto &value) {
            std::vector<int> container = {};
            value->subscribe([&](auto inner) {
                container.push_back(inner);
            });
            DEBUG_VALUE_AND_TYPE_OF(container);
        });
    //        ->subscribe(
    //            [](const rx::shared_observable<int> &win) {
    //                DEBUG_MESSAGE("new window");
    //#if 0
    //                std::vector<int> vec = {};
    //                auto o_first = std::back_inserter(vec);
    //                win->subscribe([&o_first](auto value) {
    //                    // vec.push_back(window_value);
    //                    *o_first++ = value;
    //                });
    //                DEBUG_VALUE_AND_TYPE_OF(vec);
    //#else
    //                win->to_iterable<std::vector<int>>()->subscribe([](const std::vector<int> &value) {
    //                    DEBUG_VALUE_AND_TYPE_OF(value);
    //                });
    // this causes a SIGSEGV somehow..
    // next->to_iterable<std::vector<int>>()->subscribe([](auto window_value) {
    //    DEBUG_VALUE_AND_TYPE_OF(window_value);
    //});
    //#endif
    //},
    //            [] {
    //    DEBUG_MESSAGE("windowing done");
    //            });

#if 1
    DEBUG_MESSAGE("-flat_map.and.group_by-------");

    rx::range(1, 10) //<int>(50ms)
                     //->take(10)   // 500ms
        ->flat_map<int>([](auto i) {
            return rx::of(i, i * i)->delay(100ms);
        })
        ->group_by([](auto key) {
            return key & 1;
        })
        ->subscribe(
            [](const rx::shared_observable<int> &obs) {
                DEBUG_MESSAGE("new group");
                obs->to_iterable<std::vector<int>>()->subscribe([](auto value) {
                    DEBUG_VALUE_AND_TYPE_OF(value);
                });
            },
            []() {
                std::cout << "count done!" << std::endl;
            });
    DEBUG_MESSAGE("-----------------------------");
    rx::range(1, 10)
        ->flat_map<int>([](auto val) {
            return rx::of(val)->delay(10ms)->first()->map([](auto value) {
                return value * value;
            });
        })
        ->subscribe(
            [](int value) {
                DEBUG_VALUE_OF(value);
            },
            [] {
                DEBUG_VALUE_OF("Sequence complete!");
            });
    DEBUG_MESSAGE("-----------------------------");

    std::map<int, std::chrono::milliseconds> times = {{0, 100ms}, {1, 600ms}, {2, 400ms}, {3, 700ms}, {4, 200ms}};

    rx::from(times)
        ->flat_map<int>([](const auto &time) {
            return rx::of(time.first)->delay(time.second);
        }) // 0, 2, 4
        ->debounce(500ms)
        ->subscribe([](auto value) {
            DEBUG_VALUE_OF(value);
        });
#endif
    return 0;
}
