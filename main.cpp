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

#define DEBUG_METHOD()                                                         \
    std::cout << timestamp() << " " << __PRETTY_FUNCTION__ << " @ " << this    \
              << std::endl
#define DEBUG_VALUE_OF(x)                                                      \
    std::cout << timestamp() << " " << #x << "=" << (((x))) << std::endl
#define DEBUG_MESSAGE(x) std::cout << timestamp() << " " << (((x))) << std::endl
#define DEBUG_VALUE_AND_TYPE_OF(x)                                             \
    std::cout << timestamp() << " " << #x << "=" << (((x))) << " ["            \
              << typeid((((x)))).name() << "]" << std::endl

std::string timestamp() {
    // get a precise timestamp as a string
    const auto now = std::chrono::system_clock::now();
    const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()) %
                       1000;
    std::stringstream sss;
    sss << std::put_time(std::localtime(&nowAsTimeT), "%a %b %d %Y %T") << '.'
        << std::setfill('0') << std::setw(3) << nowMs.count();
    return sss.str();
}

template <typename F>
void call_async(F &&fun) {
    auto futptr = std::make_shared<std::future<
        void>>(); // passing this by value to the lambda will increment the
                  // counter and ensure completion, even after going oos
    *futptr = std::async(std::launch::async, [futptr, fun]() { fun(); });
}

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
std::ostream &operator<<(std::ostream &os,
                         const std::chrono::duration<Rep, Period> &duration) {
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

struct tcp_listener {
    int _fd;
    int _self;
    tcp_listener(int fd) : _fd(fd) {
        _self = accept(_fd, nullptr, nullptr);
        if (_self < 0)
            perror("accept");
    }

    ~tcp_listener() { close(_self); }
};

auto tcp(uint16_t port) {
    return rx::make_shared_observable<std::string>(
        [port](const rx::observer<std::string> &on_next) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr = {0};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);
            if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
                perror("accept");
            listen(sock, 5);

            while (true) {
                // int client = accept(sock, nullptr, nullptr);
                tcp_listener client(sock);

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
                // close(client);
            }
            throw rx::on_complete();
            close(sock);
        });
}

int main() {

    tcp(5557)->take(2)->subscribe(
        [](const std::string &str) { DEBUG_VALUE_OF(str); });

    rx::interval<int>(50ms)
        ->take(10) // 500ms
        ->group_by([](auto key) { return key & 1; })
        ->subscribe(
            [](auto value) constexpr { DEBUG_VALUE_AND_TYPE_OF(value); },
            []() constexpr { std::cout << "count done!" << std::endl; });
    rx::range(1, 10)
        ->flat_map<int>([](auto val) {
            return rx::of(val, 3)->delay(10ms)->first()->map(
                [](auto value) { return value * value; });
        })
        ->subscribe([](int value) { DEBUG_VALUE_OF(value); },
                    [] { DEBUG_VALUE_OF("Sequence complete!"); });

    std::map<int, std::chrono::milliseconds> times = {
        {0, 100ms}, {1, 600ms}, {2, 400ms}, {3, 700ms}, {4, 200ms}};

    rx::from(times)
        ->flat_map<int>([](const auto &time) {
            return rx::of(time.first)->delay(time.second);
        }) // 0, 2, 4
        ->debounce(500ms)
        ->subscribe([](auto value) { DEBUG_VALUE_OF(value); });
    return 0;
}
