#if 0

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>

#define MODBUS_RTU_ADDRESS 0x01
#define MODBUS_FUNC_READ_COILS 0x01
#define MODBUS_FUNC_READ_DISCRETE_INPUTS 0x02
#define MODBUS_FUNC_READ_HOLDING_REGISTERS 0x03
#define MODBUS_FUNC_READ_INPUT_REGISTERS 0x04
#define MODBUS_FUNC_WRITE_SINGLE_COIL 0x05
#define MODBUS_FUNC_WRITE_SINGLE_REGISTER 0x06
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS 0x10


inline uint16_t crc16_update(uint16_t crc, uint8_t data)
{
  crc ^= static_cast<uint16_t>(data);
  for (int i = 0; i < 8; ++i)
  {
    crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}

uint16_t crc16(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i)
  {
    crc = crc16_update(crc, data[i]);
  }
  return crc;
}



// Modbus message parsing state
enum { STATE_IDLE, STATE_ADDRESS, STATE_FUNCTION_CODE, STATE_DATA_LENGTH, STATE_DATA, STATE_CRC } modbus_state;

template <typename InputIt, typename OutputIt>
int modbus_parse(int slave_id, InputIt first, InputIt last, OutputIt o_first) {

    OutputIt o_begin = o_first; // used to calculate buffer size

    uint8_t rtu_id = (uint8_t)slave_id;
    int len;

    int state = STATE_IDLE;
    size_t count = 0;

    for (auto it = first; it != last; ++it) {
        switch (state) {
        case STATE_IDLE:
            if (*it == rtu_id) {
                *o_first++ = *it;
                state = STATE_ADDRESS;
            }
            break;
        case STATE_ADDRESS:
            *o_first++ = *it;
            state = STATE_FUNCTION_CODE;
            break;
        case STATE_FUNCTION_CODE:
            *o_first++ = *it;
            len        = static_cast<decltype(len)>(*it);
            if(len > 255) {
                return -2;
            }
            state      = STATE_DATA_LENGTH;
            break;
        case STATE_DATA_LENGTH: {
            *o_first++ = *it;
            if(++count >= len) {
                state = STATE_DATA;
            }
            break;
        }
        case STATE_DATA: {
            uint16_t calculated_crc = crc16(o_begin, 3 + count);
            uint16_t received_crc = ntohs((*it << 8) | *(it + 1));
            if(calculated_crc != received_crc) {
                return -3;
            }
            *o_first++ = *it++;
            *o_first++ = *it;

            state = STATE_CRC;
            break;
        }

        case STATE_CRC:
            state = STATE_IDLE;
            return (int)(it != last); // true if more data available
            break;
        default:
            return -1;
        }
    }
    return 0;
}

class ModbusParser {
    static constexpr size_t MaxSizeOfModbusResponse = 230; // 3 byte header + max 255 bytes + crc16
    uint8_t _buffer[MaxSizeOfModbusResponse] = { 0 };
    off_t _off = 0;
public:
    void Parse(const uint8_t* buf, size_t buf_len) {
        do {
            int res = modbus_parse(buf, buf + buf_len, &_buffer[_off]);
        } while(res > 0);


    }
};

/*
case STATE_DATA:
buffer[*buffer_index] = data;
(*buffer_index)++;
if (*buffer_index >= message_length + 3) {
    // Check for correct CRC
    uint16_t calculated_crc = modbus_crc(buffer, message_length + 1);
    uint16_t received_crc = (buffer[*buffer_index - 2] << 8) | buffer[*buffer_index - 1];
    if (calculated_crc != received_crc) {
        fprintf(stderr, "Invalid CRC: 0x%04X expected, 0x%04X received\n", calculated_crc, received_crc);
        modbus_state = STATE_IDLE;
        return -1;
    }

    // Process the complete message
    // ...

    modbus_state = STATE_IDLE;
    return 0;
}
break;
case STATE_CRC:
// Handle CRC calculation and verification
break;
default:
fprintf(stderr, "Invalid state: %d\n", modbus_state);
modbus_state = STATE_IDLE;
return -1;
}

return 0;
}
*/
#include <iterator>
#include <vector>

int main(int, char **) {
    const uint8_t buf[] = {0x01, 0x03, 0x0e, 0x41, 0xaa, 0xc1, 0xdb, 0x00, 0x00, 0x00,
                           0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x68, 0x9a};

    uint8_t res[230] = { 0 };
    int result = modbus_parse(0x01, buf, buf + sizeof(buf), res);//std::back_inserter(res));

    if(result == 0) {
        std::cout << "all done" << std::endl;
    }

    if(result > 0) {
        std::cout << "there's still data in the buffer" << std::endl;
    }

    if(result < 0) {
        std::cout << "error code " << result << std::endl;
    }


    return 0;
}

#else

#include "refc_ptr.hpp"
#include "rx.hpp"
#include "subject.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
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
    bs.subscribe([](int z) {
        DEBUG_VALUE_AND_TYPE_OF(z);
    });
    bs.on_next(1338);
    bs.subscribe([](int a) {
        DEBUG_VALUE_AND_TYPE_OF(a);
    });

    replay_subject<int> rs(3);
    rs.subscribe([](int n) {
        DEBUG_VALUE_AND_TYPE_OF(n);
    });

    rs.on_next(1);
    rs.on_next(2);
    rs.on_next(3);

    rs.subscribe([](int m) {
        DEBUG_VALUE_AND_TYPE_OF(m);
    });

    rs.on_next(4);
    rs.subscribe([](int o) {
        DEBUG_VALUE_AND_TYPE_OF(o);
    });

    // tcp_listener(5557)->take(1)->subscribe(
    //     [](const std::string &str) { DEBUG_VALUE_OF(str); });

    // std::ifstream ifs;
    // ifs.open("test.txt");
    // rx::from_istream<char>(ifs)->to_iterable<std::string>()->subscribe([](auto c) {
    //     DEBUG_VALUE_AND_TYPE_OF(c);
    // });
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
    // #if 0
    //                std::vector<int> vec = {};
    //                auto o_first = std::back_inserter(vec);
    //                win->subscribe([&o_first](auto value) {
    //                    // vec.push_back(window_value);
    //                    *o_first++ = value;
    //                });
    //                DEBUG_VALUE_AND_TYPE_OF(vec);
    // #else
    //                win->to_iterable<std::vector<int>>()->subscribe([](const std::vector<int> &value) {
    //                    DEBUG_VALUE_AND_TYPE_OF(value);
    //                });
    // this causes a SIGSEGV somehow..
    // next->to_iterable<std::vector<int>>()->subscribe([](auto window_value) {
    //    DEBUG_VALUE_AND_TYPE_OF(window_value);
    //});
    // #endif
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
#endif
