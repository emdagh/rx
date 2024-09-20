#include "refcount_ptr.hpp"
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
    return rx::make_shared_observable<std::string>([port](const rx::observer<std::string> &on_next) {
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

    // tcp_listener(5557)->take(1)->subscribe(
    //     [](const std::string &str) { DEBUG_VALUE_OF(str); });

    std::ifstream ifs;
    ifs.open("test.txt");
    rx::from_istream<char>(ifs)->to_iterable<std::string>()->subscribe([](auto c) {
        DEBUG_VALUE_AND_TYPE_OF(c);
    });
    DEBUG_MESSAGE("-----------------------------");

    rx::of(1, 2, 3, 4, 5, 6)
        ->flat_map<int>([](auto i) {
            return rx::of(i)->delay(100ms);
        })
        ->window(250ms)
        ->subscribe(
            [](const rx::shared_observable<int> &win) {
                DEBUG_MESSAGE("new window");
#if 1
                std::vector<int> vec = {};
                auto o_first = std::back_inserter(vec);
                win->subscribe([&o_first](auto value) {
                    // vec.push_back(window_value);
                    *o_first++ = value;
                });
                DEBUG_VALUE_AND_TYPE_OF(vec);
#else
                win->to_iterable<std::vector<int>>()->subscribe(
                    [](auto value) {
                        DEBUG_VALUE_AND_TYPE_OF(value);
                    },
                    [] {
                    });
        // this causes a SIGSEGV somehow..
        // next->to_iterable<std::vector<int>>()->subscribe([](auto window_value) {
        //    DEBUG_VALUE_AND_TYPE_OF(window_value);
        //});
#endif
            },
            [] {
                DEBUG_MESSAGE("windowing done");
            });

    DEBUG_MESSAGE("-----------------------------");

    rx::range(1, 10) //<int>(50ms)
                     //->take(10)   // 500ms
        ->flat_map<int>([](auto i) {
            return rx::of(i)->delay(100ms);
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
    return 0;
}
