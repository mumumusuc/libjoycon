/*
 *   Copyright (c) 2020 mumumusuc

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "controller.h"
#include "crc8.h"
#include "input_report.h"
#include "log.h"
#include "output_report.h"
#include <assert.h>
#include <chrono>
#include <future>
#include <math.h>
#include <stdarg.h>

#define RETRY 10
#define DEBUG 1
#if DEBUG
#define debug(fmt, ...) log_d(__func__, fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

using namespace controller;
using namespace session;
using GuardLock = std::lock_guard<std::mutex>;
using SessionSp = std::unique_ptr<Session>;

const DeviceDesc sNintendoSwitch = {
    .role = CONSOLE,
    .name = std::string("Nintendo Switch").c_str(),
    .mac_address = std::string("DC:68:EB:15:9A:62").c_str(),
    .serial_number = std::string("").c_str(),
};

class Counter {
  private:
    using time_point = std::chrono::system_clock::time_point;
    using Printer = std::function<void(float)>;
    time_point begin_;
    Printer printer_;

  public:
    Counter() noexcept : begin_(std::chrono::system_clock::now()), printer_(nullptr){};

    Counter(const Counter &) = delete;

    ~Counter() {
        auto end = std::chrono::system_clock::now();
        auto duration = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(end - begin_).count());
        if (printer_)
            printer_(duration);
        else
            debug("cost %.2f ms", duration);
    };

    Counter &operator=(const Counter &) = delete;
};

template <typename T>
static inline int transmit(unsigned retry, const void *buffer, Inspector inspector,
                           std::vector<std::future<Result>> &result, const T &session) {
    result.emplace_back(std::move(session->Transmit(retry, buffer, inspector)));
    return 0;
}

template <typename T>
static inline int aquire(unsigned retry, Inspector inspector,
                         std::vector<std::future<Result>> &result, const T &session) {
    result.emplace_back(std::move(session->Acquire(retry, inspector)));
    return 0;
}

template <typename... Args>
static inline void nop(Args... args) {}

template <typename... Args>
inline void
ControllerImpl::Transmit(unsigned retry, const void *buffer, Inspector inspector, const Args &... sessions) {
    results_.clear();
    assert(results_.empty());
    nop(transmit(retry, buffer, inspector, results_, sessions)...);
}

inline int ControllerImpl::Await() {
    int ret = 0;
    for (auto it = results_.begin(); it != results_.end(); ++it)
        ret = (*it).get();
    return ret;
}

ControllerImpl::ControllerImpl(const Device *host) : host_(*host) {
    int ret = 0;
    results_.reserve(8);
    output_ = reinterpret_cast<OutputReport *>(calloc(1, OUTPUT_REPORT_SIZE));
    if (output_ == nullptr) {
        throw std::runtime_error(strerror(ENOMEM));
    }
}

ControllerImpl::~ControllerImpl() {
    int ret = 0;
    results_.clear();
    free(output_);
    assert(ret == 0);
}

template <typename... Args>
int ControllerImpl::Pair(const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_01 = SUBCMD_01_INIT;
        output_->subcmd_01.subcmd = 0x4;
        output_->subcmd_01.address = str_to_mac_address_le(host_.desc.mac_address);
        output_->subcmd_01.alias = alias(host_.desc.name);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_01)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::Poll(PollType type, const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_03 = SUBCMD_03_INIT;
        output_->subcmd_03.poll_type = type;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            //hex_dump("POLL", buffer, INPUT_REPORT_STAND_SIZE);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_03)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::GetData(ControllerData &data, const Args &... sessions) {
    debug();
    int ret = 0;
    auto inspector = [&data](const void *input) -> int {
        auto buffer = static_cast<const InputReport *>(input);
        if (buffer->id == 0x30 || buffer->id == 0x21 || buffer->id == 0x31) {
            if (buffer->controller_state.category == PRO_GRIP)
                data = buffer->controller_data;
            else
                controller_data_merge(&data, &buffer->controller_data);
            return DONE;
        }
        return WAITING;
    };
    Transmit(RETRY, nullptr, inspector, sessions...);
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetPlayer(Player player, PlayerFlash flash, const Args &... sessions) {
    debug();
    int ret = 0;
    Counter counter;
    GuardLock lock(sess_lock_);
    {
        GuardLock _1(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_30 = SUBCMD_30_INIT;
        output_->subcmd_30.player = player;
        output_->subcmd_30.flash = flash;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_30)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetLowPower(bool enable, const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_08 = SUBCMD_08_INIT;
        output_->subcmd_08.enable = enable ? 0x1 : 0x0;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            //hex_dump("LOW", buffer, INPUT_REPORT_STAND_SIZE);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_08)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetElapsedTime(uint8_t time, const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_04 = SUBCMD_04_INIT;
        output_->subcmd_04.time = u16_le(0);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_04)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetImu(bool enable, const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_40 = SUBCMD_40_INIT;
        output_->subcmd_40.enable = enable ? 0x1 : 0x0;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_40)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::ReadMemory(uint32_t address, uint8_t size, void *data, const Args &... sessions) {
    debug();
    int ret = 0;
    if (!assert_flash_mem_address(address))
        return -EINVAL;
    if (!assert_flash_mem_length(size))
        return -EINVAL;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_10 = SUBCMD_10_INIT;
        output_->subcmd_10.address = address;
        output_->subcmd_10.length = size;
        auto inspector = [&address, &size, &data](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_10) {
                uint32_t _address = le32(buffer->reply.data);
                uint8_t _size = buffer->reply.data[4];
                debug("address = %08x, length = %02hhx", _address, _size);
                if (_address == address && _size == size) {
                    size_t offset = sizeof(uint32_t) + sizeof(uint8_t);
                    //hex_dump("FLASH", buffer->reply.data + offset, size);
                    memmove(data, buffer->reply.data + offset, size);
                    return DONE;
                }
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::WriteMemory(uint32_t address, uint8_t size, const void *data, const Args &... sessions) {
    debug();
    int ret = 0;
    if (!assert_flash_mem_address(address))
        return -EINVAL;
    if (!assert_flash_mem_length(size))
        return -EINVAL;
    GuardLock _1(sess_lock_);
    {
        GuardLock _2(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_11 = SUBCMD_11_INIT;
        output_->subcmd_11.address = address;
        output_->subcmd_11.length = size;
        memmove(output_->subcmd_11.data, data, size);
        auto inspector = [&address, &size, &data](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_11) {
                uint8_t status = buffer->reply.data[0];
                debug("status = %02hhx", status);
                if (status == 0)
                    // success
                    return DONE;
                else
                    return ERROR;
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

//#define min(x, y) (x) < (y) ? (x) : (y)
template <typename T>
static constexpr T min(T x, T y) {
    return x < y ? x : y;
}

template <typename... Args>
int ControllerImpl::BackupMemory(Progress progress, const Args &... sessions) {
    debug();
    int ret = 0;
    /*
    time_t t;
    time(&t);
    char filename[32] = {'\0'};
    sprintf(filename, "flash_%ld.bin", t);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        return -EBADFD;
    }
    */
    int32_t address = 0;
    uint8_t size = FLASH_MEM_STEP;
    uint8_t buffer[FLASH_MEM_STEP];
    while (address < FLASH_MEM_SIZE) {
        bzero(buffer, sizeof(buffer));
        size = min(FLASH_MEM_SIZE - address, FLASH_MEM_STEP);
        ret = ReadMemory(address, size, buffer, sessions...);
        if (ret != DONE) {
            debug("readFlashMemory -> %d", ret);
            goto failed;
        }
        /*
        ret = fwrite(buffer, size, 1, file);
        if (ret <= 0) {
            goto failed;
        }
        */
        address += size;
        if (progress)
            progress(FLASH_MEM_SIZE, address);
    }
    // fclose(file);
    ret = address;
    return ret;

failed:
    // fclose(file);
    return ret;
}

template <typename... Args>
int ControllerImpl::RestoreMemory(Progress progress, const Args &... sessions) {
    throw std::runtime_error("not implemented");
    return 0;
}

template <typename... Args>
int ControllerImpl::GetColor(ControllerColor &color, const Args &... sessions) {
    return this->ReadMemory(0x6050, sizeof(ControllerColor), reinterpret_cast<void *>(&color), sessions...);
}

template <typename... Args>
int ControllerImpl::SetColor(const ControllerColor &color, const Args &... sessions) {
    return this->WriteMemory(0x6050, sizeof(ControllerColor), reinterpret_cast<const void *>(&color), sessions...);
}

template <typename... Args>
int ControllerImpl::SetRumble(bool enable, const Args &... sessions) {
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_48 = SUBCMD_48_INIT;
        output_->subcmd_48.enable_vibration = enable ? 0x1 : 0;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_48)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::Rumble(const rumble_data_t *left, const rumble_data_t *right,
                           const Args &... sessions) {
    if (!left && !right) return 0;
    int ret = 0;
    {
        GuardLock _1(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_RUM;
        if (left)
            output_->rumble.rumble_l = *left;
        if (right)
            output_->rumble.rumble_r = *right;
        Transmit(RETRY, output_, nullptr, sessions...);
    }
    ret = Await();
    return ret;
}

static inline float _freq_amp(float a) {
    if (a < 0.117471f) {
        return 0.0005f * a * a;
    } else if (a < 0.229908f) {
        return log2f(a * 17.f) * 16.f;
    } else if (a > 1.0) {
        // not safe
        return 100;
    } else {
        return log2f(a * 8.7f) * 32.f;
    }
}

static int
CalcRumblef(rumble_data_t &rumble, float freq_h, float freq_h_amp, float freq_l, float freq_l_amp) {
    if (freq_h < 80.f || freq_h > 1252.0f || freq_h_amp < 0 || freq_h_amp > 1)
        return -EINVAL;
    if (freq_l < 40.f || freq_l > 626.0f || freq_l_amp < 0 || freq_l_amp > 1)
        return -EINVAL;
    auto freq_h_hex = static_cast<uint8_t>(roundf(log2f(freq_h / 10.0f) * 32.0f));
    auto freq_l_hex = static_cast<uint8_t>(roundf(log2f(freq_l / 10.0f) * 32.0f));
    auto _freq_h = static_cast<uint16_t>((freq_h_hex - 0x60) << 2);
    auto _freq_l = static_cast<uint8_t>(freq_l_hex - 0x40);
    auto k_h = static_cast<uint8_t>(roundf(_freq_amp(freq_h_amp)));
    auto k_l = static_cast<uint8_t>(roundf(_freq_amp(freq_l_amp)));
    auto _freq_h_amp = static_cast<uint8_t>(k_h * 2);
    auto msb = static_cast<uint16_t>((k_l & 0x1) << 15);
    auto _freq_l_amp = static_cast<uint16_t>(((k_l >> 1) | msb) + 0x0040);
    debug("(%.04f,%04x) (%.04f,%02hhx)", freq_h, _freq_h, freq_h_amp, _freq_h_amp);
    debug("(%.04f,%04x) (%.04f,%04x)", freq_l, _freq_l, freq_l_amp, _freq_l_amp);
    rumble.freq_h = (uint8_t)(_freq_h & 0xff);
    rumble.freq_h_amp = (uint8_t)(_freq_h_amp | ((_freq_h >> 8) & 0xff));
    rumble.freq_l = (uint8_t)(_freq_l | ((_freq_l_amp >> 8) & 0xff));
    rumble.freq_l_amp = (uint8_t)(_freq_l_amp & 0xff);
    return 0;
}

template <typename... Args>
int ControllerImpl::Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right,
                            const Args &... sessions) {
    rumble_t rumble = {};
    if (left)
        CalcRumblef(rumble.rumble_l, left->freq_h, left->freq_h_amp, left->freq_l,
                    left->freq_l_amp);
    if (right)
        CalcRumblef(rumble.rumble_r, right->freq_h, right->freq_h_amp, right->freq_l,
                    right->freq_l_amp);
    return Rumble(&rumble.rumble_l, &rumble.rumble_r, sessions...);
}

template <typename... Args>
int ControllerImpl::SetMcuState(McuState state, const Args &... sessions) {
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_22 = SUBCMD_22_INIT;
        output_->subcmd_22.state = state;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_22)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::CheckMcuMode(McuMode mode, const Args &... sessions) {
    int ret = 0;
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd.cmd = 0x1;
        auto inspector = [&mode](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            // if (ir[49] == 0x01 && ir[7] == 0x06) // MCU state is Initializing
            // TODO: rewrite ir state :
            // ir[0] -> reply
            // ir[1:2] -> ?
            // ir[3:4] -> fw things, LE x04 in lower than 3.89fw, x05 in 3.89
            // ir[5:6] -> fw things, LE x12 in lower than 3.89fw, x18 in 3.89
            // ir[7]   -> mcu state @see McuMode_t
            // hex_dump("STEP4", buffer, SESSION_RECV_BUFFER_SIZE);
            // debug("checkMcuMode -> %u", buffer->ir[7]);
            if (buffer->id == 0x31 && buffer->ir[0] == 0x01 && buffer->ir[7] == mode)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetMcuMode(McuMode mode, const Args &... sessions) {
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_21 = SUBCMD_21_INIT;
        output_->subcmd_21.subcmd = MCU_CMD_SET_MODE;
        output_->subcmd_21.mcu_subcmd = McuSubcmd(0);
        output_->subcmd_21.mcu_mode = mode;
        calc_crc8_21(output_);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            // Mcu mode is standby
            // data[0]   -> 1,ok?
            // data[1:2] -> ?
            // data[3:4] -> fw things, LE x04 in lower than 3.89fw, x05 in 3.89
            // data[5:6] -> fw things, LE x04 in lower than 3.89fw, x05 in 3.89
            // data[7]   -> 0x1, standby
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_21 &&
                buffer->reply.data[0] == 0x1 && buffer->reply.data[7] == 0x1)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

// ProController & JoyCon_R

template <typename... Args>
int ControllerImpl::SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size,
                                 const HomeLightPattern *patterns, const Args &... sessions) {
    debug();
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_38 = SUBCMD_38_INIT;
        output_->subcmd_38.base_duration = duration;
        output_->subcmd_38.start_intensity = intensity;
        output_->subcmd_38.pattern_count = size;
        output_->subcmd_38.repeat_count = repeat;
        output_->subcmd_38.patterns = home_light_pattern(patterns, size);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_38)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
};

// JoyCon_R
template <typename... Args>
int ControllerImpl::CheckMcuIrMode(IrMode mode, const Args &... sessions) {
    int ret = 0;
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd_03 = SUBCMD_03_INIT;
        output_->subcmd_03.poll_type = POLL_NFC_IR_DATA;
        output_->subcmd_03.tail = 0xff;
        calc_crc8_03(output_);
        auto inspector = [&mode](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            // hex_dump("STEP6", buffer, SESSION_RECV_BUFFER_SIZE);
            if (buffer->id == 0x31 && buffer->ir[0] == 0x13 &&
                buffer->ir[1] == 0x0 && buffer->ir[2] == mode)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::SetMcuIrRegisters(const McuReg *regs, size_t size, const Args &... sessions) {
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_21 = SUBCMD_21_INIT;
        output_->subcmd_21.subcmd = MCU_CMD_WRITE;
        output_->subcmd_21.mcu_subcmd = MCU_SET_IR_REG;
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_21)
                return DONE;
            return WAITING;
        };
        int count = 0;
        uint8_t trunk = 0;
        auto dist = reinterpret_cast<void *>(output_->subcmd_21.reg);
        //auto src = reinterpret_cast<const void *>(regs);
        debug("size -> %zu", size);
        while (count < size) {
            trunk = (uint8_t)min(size_t(9), size - count);
            output_->subcmd_21.mcu_reg_size = trunk;
            bzero(dist, sizeof(McuReg) * 9);
            memmove(dist, regs + count, sizeof(McuReg) * trunk);
            calc_crc8_21(output_);
            Transmit(RETRY, output_, inspector, sessions...);
            ret = Await();
            if (ret != DONE)
                break;
            count += trunk;
        };
    }
    return ret;
}

template <typename... Args>
int ControllerImpl::SetMcuIrConfig(const IrConfigFixed &fixed, const Args &... sessions) {
    int ret = 0;
    GuardLock lock(sess_lock_);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_CMD;
        output_->subcmd_21 = SUBCMD_21_INIT;
        output_->subcmd_21.subcmd = MCU_CMD_WRITE;
        output_->subcmd_21.mcu_subcmd = MCU_SET_IR_MODE;
        output_->subcmd_21.mcu_ir_mode = fixed.mode;
        output_->subcmd_21.mcu_fragments = fixed.fragments;
        output_->subcmd_21.mcu_major_v = fixed.major;
        output_->subcmd_21.mcu_minor_v = fixed.minor;
        calc_crc8_21(output_);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x21 && buffer->reply.subcmd_id == SUBCMD_21 &&
                buffer->reply.data[0] == 0xb)
                return DONE;
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    if (ret != DONE)
        return ret;
    const McuReg regs[] = {
        set_reg(MCU_REG_ADDR_RESOLUTION, fixed.resolution),
        set_reg(MCU_REG_ADDR_UPDATE_TIME, fixed.update_time),
        REG_FINALIZE,
    };
    ret = SetMcuIrRegisters(regs, sizeof(regs) / sizeof(McuReg), sessions...);
    return ret;
}

template <typename... Args>
int ControllerImpl::SetMcuIrConfig(const IrConfigLive &live, const Args &... sessions) {
    int ret = 0;
    const McuReg regs[] = {
        set_reg(MCU_REG_ADDR_EXP_TIME_LSB, live.exposure._0),
        set_reg(MCU_REG_ADDR_EXP_TIME_MSB, live.exposure._1),
        set_reg(MCU_REG_ADDR_EXP_TIME_MAX, EXP_TIME_MAX_MANUAL),
        set_reg(MCU_REG_ADDR_LEDS_STATE, live.leds),
        set_reg(MCU_REG_ADDR_DIGI_GAIN_LSB, (live.digi_gain & 0x0f) << 4),
        set_reg(MCU_REG_ADDR_DIGI_GAIN_MSB, (live.digi_gain & 0xf0) >> 4),
        set_reg(MCU_REG_ADDR_EXT_LIGHT_FILTER, live.ex_light_filter),
        set_reg(MCU_REG_ADDR_EXLF_THR, EXLF_THR_DEFAULT),
        set_reg(MCU_REG_ADDR_LEDS_1_2_INT, live.intensity.bright),
        set_reg(MCU_REG_ADDR_LEDS_3_4_INT, live.intensity.dim),
        set_reg(MCU_REG_ADDR_FLIP_IMG, live.flip),
        set_reg(MCU_REG_ADDR_DENOISE_ALG, live.denoise.enable),
        set_reg(MCU_REG_ADDR_DENOISE_EDGE, live.denoise.edge),
        set_reg(MCU_REG_ADDR_DENOISE_COLOR, live.denoise.color),
        REG_FINALIZE,
    };
    ret = SetMcuIrRegisters(regs, sizeof(regs) / sizeof(McuReg), sessions...);
    return ret;
}

template <typename... Args>
int ControllerImpl::SetIrConfig(const IrConfig &config, uint8_t *image, IrCallback cb,
                                const Args &... sessions) {
    int ret = 0;
    // 0. set input report to 0x31
    ret = Poll(POLL_NFC_IR, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 0 done");
    // 1. enable MCU
    ret = SetMcuState(MCU_STATE_RESUME, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 1 done");
    // 2. request MCU mode status
step_2:
    ret = CheckMcuMode(MCU_MODE_STANDBY, sessions...);
    // assert(ret == DONE);
    if (ret == TIMEDOUT)
        goto step_2;
    else if (ret != DONE)
        goto step_10;
    debug("--------------------------- step 2 done");
    // 3. set MCU mode
    ret = SetMcuMode(MCU_MODE_IR, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 3 done");
    // 4. request MCU mode status
    ret = CheckMcuMode(MCU_MODE_IR, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 4 done");
    // 5. Set IR mode and number of packets for each data blob. Blob size is
    // packets * 300 bytes.
    ret = SetMcuIrConfig(config.fixed, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 5 done");
// 6. Request IR mode status
step_6:
    ret = CheckMcuIrMode(IR_MODE_IMG_TRANSFER, sessions...);
    // assert(ret == DONE);
    if (ret == TIMEDOUT)
        goto step_6;
    else if (ret != DONE)
        goto step_10;
    debug("--------------------------- step 6 done");
    // 7,8
    ret = SetMcuIrConfig(config.live, sessions...);
    assert(ret == DONE);
    debug("--------------------------- step 7,8 done");
    // 9. Stream or Capture images from NIR Camera
    ret = GetIrImage(config.fixed, image, cb, sessions...);
    debug("--------------------------- step 9 done");
step_10:
    // Disable MCU
    ret = SetMcuState(MCU_STATE_SUSPEND, sessions...);
    assert(ret == DONE);
    // Set input report back to x3f
    ret = Poll(POLL_STANDARD, sessions...);
    assert(ret == DONE);
    return ret;
}

template <typename... Args>
int ControllerImpl::GetIrImage(const IrConfigFixed &fixed, uint8_t *image, IrCallback cb,
                               const Args &... sessions) {
    debug();
    int ret = 0;
    int pre_frag_no = 0;
    int cur_frag_no = 0;
    int ir_max_frag = fixed.fragments;
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd_03 = SUBCMD_03_INIT;
        output_->subcmd_03.poll_type = POLL_NFC_IR_CAM;
        output_->subcmd_03.tail = 0xff;
        calc_crc8_03(output_);
        auto inspector = [this, &sessions..., &pre_frag_no, &cur_frag_no, &ir_max_frag, &image, &cb](
                             const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            //hex_dump("IR", buffer, INPUT_REPORT_LARGE_SIZE);
            if (buffer->id == 0x31 && buffer->ir[0] == 0x3) {
                // avaliable
                cur_frag_no = buffer->ir[3];
                debug("cur_frag_no = %u", cur_frag_no);
                if (cur_frag_no == 0) {
                    // fragment begin
                    pre_frag_no = 0;
                    memmove(image + 300 * cur_frag_no, buffer->ir + 10, 300);
                } else if (cur_frag_no == pre_frag_no) {
                    // duplicated
                    debug("duplicated fragment, skip");
                } else if (cur_frag_no == ir_max_frag) {
                    // fragment end
                    pre_frag_no = cur_frag_no;
                    memmove(image + 300 * cur_frag_no, buffer->ir + 10, 300);
                    if (cb && cb() != 0)
                        return DONE;
                    bzero(image, (ir_max_frag + 1) * 300);
                } else if (cur_frag_no == pre_frag_no + 1) {
                    // fragment next
                    pre_frag_no = cur_frag_no;
                    memmove(image + 300 * cur_frag_no, buffer->ir + 10, 300);
                }
                output_->subcmd_03.raw[3] = cur_frag_no;
                calc_crc8_03(output_);
                debug("ack for fragment %u", cur_frag_no);
                Transmit(0, output_, nullptr, sessions...);
                Await();
                return AGAIN;
            } else if (buffer->id == 0x31) {
                // Empty IR report. Send Ack again. Otherwise, it fallbacks to
                // high latency mode (30ms per data fragment)
                if (buffer->ir[0] == 0xff) {
                    // send ACK again
                    debug("got ff, resend pre_frag_no -> %u", pre_frag_no);
                    output_->subcmd_03.raw[1] = 0x0;
                    output_->subcmd_03.raw[2] = 0x0;
                    output_->subcmd_03.raw[3] = pre_frag_no;
                } else if (buffer->ir[0] == 0x0) {
                    // request missed frag
                    debug("got 00, resend pre_frag_no+1 -> %u", pre_frag_no + 1);
                    output_->subcmd_03.raw[1] = 0x1;
                    output_->subcmd_03.raw[2] = pre_frag_no + 1;
                    output_->subcmd_03.raw[3] = 0x0;
                }
                calc_crc8_03(output_);
                Transmit(0, output_, nullptr, sessions...);
                Await();
                return AGAIN;
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::TestIR(int mode, uint8_t *image, IrCallback cb, const Args &... sessions) {
    ir_config_t config;
    switch (mode) {
    case 0:
        config = {
            .fixed = ir_config_240p,
            .live =
                {
                    .exposure = ir_exposure_us(100),
                    .leds = IR_CONFIG_LED_BRIGHT | IR_CONFIG_LED_STROBE,
                    .intensity = {0x70, 0x70},
                    .ex_light_filter = IR_CONFIG_EX_FILTER_OFF,
                    .digi_gain = 1,
                    .denoise = {0, 0x7f, 0x7f},
                    .flip = IR_CONFIG_FLIP_NORM,
                },
        };
        break;
    case 1:
        config = {
            .fixed = ir_config_120p,
            .live =
                {
                    .exposure = ir_exposure_us(100),
                    .leds = IR_CONFIG_LED_BRIGHT | IR_CONFIG_LED_STROBE,
                    .intensity = {0x70, 0x70},
                    .ex_light_filter = IR_CONFIG_EX_FILTER_OFF,
                    .digi_gain = 1,
                    .denoise = {0, 0x7f, 0x7f},
                    .flip = IR_CONFIG_FLIP_NORM,
                },
        };
        break;
    case 2:
        config = {
            .fixed = ir_config_60p,
            .live =
                {
                    .exposure = ir_exposure_us(100),
                    .leds = IR_CONFIG_LED_BRIGHT | IR_CONFIG_LED_STROBE,
                    .intensity = {0x70, 0x70},
                    .ex_light_filter = IR_CONFIG_EX_FILTER_OFF,
                    .digi_gain = 1,
                    .denoise = {0, 0x7f, 0x7f},
                    .flip = IR_CONFIG_FLIP_NORM,
                },
        };
        break;
    case 3:
        config = {
            .fixed = ir_config_30p,
            .live =
                {
                    .exposure = ir_exposure_us(100),
                    .leds = IR_CONFIG_LED_BRIGHT | IR_CONFIG_LED_STROBE,
                    .intensity = {0x70, 0x70},
                    .ex_light_filter = IR_CONFIG_EX_FILTER_OFF,
                    .digi_gain = 1,
                    .denoise = {0, 0x7f, 0x7f},
                    .flip = IR_CONFIG_FLIP_NORM,
                },
        };
        break;
    default:
        config = ir_test_config;
        break;
    }
    return SetIrConfig(config, image, cb, sessions...);
}

#define calc_crc8_2(buff) (buff)->subcmd.raw[36] = crc8((buff)->subcmd.raw, 36);

template <typename... Args>
int ControllerImpl::SetMcuNfcConfig(const Args &... sessions) {
    int ret = 0;
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd.cmd = 0x2;
        output_->subcmd.raw[0] = 0x4; // 0: Cancel all, 4: StartWaitingReceive
        // Count of the currecnt packet if the cmd is a series of packets.
        output_->subcmd.raw[1] = 0x0;
        output_->subcmd.raw[2] = 0x0;
        output_->subcmd.raw[3] = 0x8;
        output_->subcmd.raw[4] = 0x0;
        calc_crc8_2(output_);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x31 && buffer->nfc[0] == 0x2a &&
                buffer->nfc[1] == 0x0 && buffer->nfc[2] == 0x5 &&
                buffer->nfc[6] == 0x31) {
                uint8_t mode = buffer->nfc[7];
                debug("nfc ack -> %02hhx", mode);
                // 0x00: Awaiting cmd, 0x0b: Initializing/Busy
                if (mode == 0x0)
                    return DONE;
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    assert(ret == DONE);
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd.cmd = 0x2;
        output_->subcmd.raw[0] = 0x1; // 1: Start polling, 2: Stop polling,
        // Count of the currecnt packet if the cmd is a series of packets.
        output_->subcmd.raw[1] = 0x0;
        output_->subcmd.raw[2] = 0x0;
        // 8: Last cmd packet, 0: More cmd packet should be expected
        output_->subcmd.raw[3] = 0x8;
        output_->subcmd.raw[4] = 0x5;  // Length of data after cmd header
        output_->subcmd.raw[5] = 0x01; // 1: Enable Mifare support
        output_->subcmd.raw[6] = 0x00; // Unknown.
        output_->subcmd.raw[7] = 0x00; // Unknown.
        // Unknown. Some values work (0x07) other don't.
        output_->subcmd.raw[8] = 0x2c;
        // Unknown. This is not needed but Switch sends it.
        output_->subcmd.raw[9] = 0x01;
        calc_crc8_2(output_);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x31 && buffer->nfc[0] == 0x2a &&
                buffer->nfc[1] == 0x0 && buffer->nfc[2] == 0x5) {
                // nfc[0] == 0x2a: NFC MCU input report
                // nfc[1] shows when there's error?
                // nfc[2] == 0x05: NFC
                // nfc[5] always 9?
                // nfc[6] always x31?
                // nfc[7]: MCU/NFC state
                // nfc[13]: nfc tag IC
                // nfc[14]: nfc tag Type
                // nfc[15]: size of following data and it's the last NFC header byte
                uint8_t mode = buffer->nfc[7];
                debug("nfc ack -> %02hhx", mode);
                // 0x09: Tag detected
                if (mode == 0x9) {
                    uint8_t tag_uid[10];
                    uint8_t tag_uid_size = buffer->nfc[15];
                    debug("tag_type -> %s", buffer->nfc[13] == 0x2 ? "NTAG" : "MIFARE");
                    debug("tag_uid_size -> %u", tag_uid_size);
                    memmove(tag_uid, buffer->nfc + 16, tag_uid_size);
                    hex_d("TAG", tag_uid, tag_uid_size);
                    return DONE;
                }
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::GetNfcNtag(const Args &... sessions) {
    int ret = 0;
    {
        GuardLock lock(output_lock_);
        bzero(output_, OUTPUT_REPORT_SIZE);
        output_->id = OUTPUT_REPORT_PHL;
        output_->subcmd.cmd = 0x2;
        output_->subcmd.raw[0] = 0x6; // 6: Read Ntag data, 0xf: Read mifare data
        output_->subcmd.raw[1] = 0x0;
        output_->subcmd.raw[2] = 0x0;
        output_->subcmd.raw[3] = 0x8;
        output_->subcmd.raw[4] = 0x13;  // Length of data after cmd header
        output_->subcmd.raw[5] = 0xd0;  // Unknown
        output_->subcmd.raw[6] = 0x07;  // Unknown or UID lentgh?
        output_->subcmd.raw[7] = 0x00;  // Only for Mifare cmds or should have a UID?
        output_->subcmd.raw[8] = 0x00;  // should have a UID?
        output_->subcmd.raw[9] = 0x00;  // should have a UID?
        output_->subcmd.raw[10] = 0x00; // should have a UID?
        output_->subcmd.raw[11] = 0x00; // should have a UID?
        output_->subcmd.raw[12] = 0x00; // should have a UID?
        output_->subcmd.raw[13] = 0x00; // should have a UID?
        // 1: Ntag215 only. 0: All tags, otherwise error x48 (Invalid format error)
        output_->subcmd.raw[14] = 0x00;
        // https://www.tagnfc.com/en/info/11-nfc-tags-specs
        // If the following is selected wrongly, error x3e (Read error)
        uint8_t ntag_pages = 0;
        switch (ntag_pages) {
        case 0:
            output_->subcmd.raw[15] = 0x01;
            break;
            // Ntag213
        case 45:
            // The following 7 bytes should be decided with max the current ntag
            // pages and what we want to read.
            output_->subcmd.raw[15] =
                0x01; // How many blocks to read. Each block should be <= 60
            // pages (240 bytes)? Min/Max values are 1/4, otherwise
            // error x40 (Argument error)
            output_->subcmd.raw[16] = 0x00; // Block 1 starting page
            output_->subcmd.raw[17] = 0x2C; // Block 1 ending page
            output_->subcmd.raw[18] = 0x00; // Block 2 starting page
            output_->subcmd.raw[19] = 0x00; // Block 2 ending page
            output_->subcmd.raw[20] = 0x00; // Block 3 starting page
            output_->subcmd.raw[21] = 0x00; // Block 3 ending page
            output_->subcmd.raw[22] = 0x00; // Block 4 starting page
            output_->subcmd.raw[23] = 0x00; // Block 4 ending page
            break;
            // Ntag215
        case 135:
            // The following 7 bytes should be decided with max the current ntag
            // pages and what we want to read.
            output_->subcmd.raw[15] =
                0x03; // How many page ranges to read. Each range should be
            // <= 60 pages (240 bytes)? Max value is 4.
            output_->subcmd.raw[16] = 0x00; // Block 1 starting page
            output_->subcmd.raw[17] = 0x3b; // Block 1 ending page
            output_->subcmd.raw[18] = 0x3c; // Block 2 starting page
            output_->subcmd.raw[19] = 0x77; // Block 2 ending page
            output_->subcmd.raw[20] = 0x78; // Block 3 starting page
            output_->subcmd.raw[21] = 0x86; // Block 3 ending page
            output_->subcmd.raw[22] = 0x00; // Block 4 starting page
            output_->subcmd.raw[23] = 0x00; // Block 4 ending page
            break;
        case 231:
            // The following 7 bytes should be decided with max the current ntag
            // pages and what we want to read.
            output_->subcmd.raw[15] =
                0x04; // How many page ranges to read. Each range should be
            // <= 60 pages (240 bytes)? Max value is 4.
            output_->subcmd.raw[16] = 0x00; // Block 1 starting page
            output_->subcmd.raw[17] = 0x3b; // Block 1 ending page
            output_->subcmd.raw[18] = 0x3c; // Block 2 starting page
            output_->subcmd.raw[19] = 0x77; // Block 2 ending page
            output_->subcmd.raw[20] = 0x78; // Block 3 starting page
            output_->subcmd.raw[21] = 0xB3; // Block 3 ending page
            output_->subcmd.raw[22] = 0xB4; // Block 4 starting page
            output_->subcmd.raw[23] = 0xE6; // Block 4 ending page
            break;
        default:
            break;
        }
        calc_crc8_2(output_);
        auto inspector = [](const void *input) -> int {
            auto buffer = static_cast<const InputReport *>(input);
            if (buffer->id == 0x31 && buffer->nfc[0] == 0x2a &&
                buffer->nfc[1] == 0x0 && buffer->nfc[2] == 0x5 &&
                buffer->nfc[6] == 0x31) {
                uint8_t mode = buffer->nfc[7];
                debug("nfc ack -> %02hhx", mode);
                // 0x00: Awaiting cmd, 0x0b: Initializing/Busy
                if (mode == 0x0)
                    return DONE;
            }
            return WAITING;
        };
        Transmit(RETRY, output_, inspector, sessions...);
    }
    ret = Await();
    return ret;
}

template <typename... Args>
int ControllerImpl::GetNfcData(const Args &... sessions) {
    int ret = 0;
    // 0. set input report to 0x31
    ret = Poll(POLL_NFC_IR);
    assert(ret == DONE);
    debug("--------------------------- step 0 done");
    // 1. enable MCU
    ret = SetMcuState(MCU_STATE_RESUME);
    assert(ret == DONE);
    debug("--------------------------- step 1 done");
// 2. request MCU mode status
step_2:
    ret = CheckMcuMode(MCU_MODE_STANDBY);
    if (ret == TIMEDOUT)
        goto step_2;
    else if (ret != DONE)
        goto step_10;
    debug("--------------------------- step 2 done");
    // 3. set MCU mode
    ret = SetMcuMode(MCU_MODE_NFC);
    assert(ret == DONE);
    debug("--------------------------- step 3 done");
    // 4. request MCU mode status
    ret = CheckMcuMode(MCU_MODE_NFC);
    assert(ret == DONE);
    debug("--------------------------- step 4 done");
    // 5,6.  Request NFC mode status
    ret = SetMcuNfcConfig();
    assert(ret == DONE);
    debug("--------------------------- step 5,6 done");
    // 7. Read NTAG contents
    // ret = this->setMcuIrConfig(&config->live);
    assert(ret == DONE);
    debug("--------------------------- step 7,8 done");
    // 9. Stream or Capture images from NIR Camera
    // this->getIrImage(&config->fixed, image, callback);
    debug("--------------------------- step 9 done");
step_10:
    // Disable MCU
    ret = SetMcuState(MCU_STATE_SUSPEND);
    assert(ret == DONE);
    // Set input report back to x3f
    ret = Poll(POLL_SIMPLE_HID);
    assert(ret == DONE);
    return ret;
}

JoyCon_L::JoyCon_L(const Device &host) : JoyCon_L(new ControllerImpl(&host)){};

JoyCon_L::JoyCon_L(ControllerImpl *impl) : impl_(impl) {
    session_ = std::unique_ptr<Session>(impl_->OpenDevice(1, PID));
};

int JoyCon_L::Pair() { return impl_->Pair(session_); };
int JoyCon_L::Poll(PollType type) { return impl_->Poll(type, session_); };
int JoyCon_L::BackupMemory(Progress progress) { return impl_->BackupMemory(progress, session_); };
int JoyCon_L::RestoreMemory(Progress progress) { return impl_->RestoreMemory(progress, session_); };
int JoyCon_L::GetData(ControllerData &data) { return impl_->GetData(data, session_); };
int JoyCon_L::GetColor(ControllerColor &color) { return impl_->GetColor(color, session_); };
int JoyCon_L::SetColor(const ControllerColor &color) { return impl_->SetColor(color, session_); };
int JoyCon_L::SetLowPower(bool enable) { return impl_->SetLowPower(enable, session_); };
int JoyCon_L::SetPlayer(Player player, PlayerFlash flash) {
    return impl_->SetPlayer(player, flash, session_);
};
int JoyCon_L::SetImu(bool enable) { return impl_->SetImu(enable, session_); };
int JoyCon_L::SetRumble(bool enable) { return impl_->SetRumble(enable, session_); };
int JoyCon_L::Rumble(const rumble_data_t *left, const rumble_data_t *right) {
    return impl_->Rumble(left, nullptr, session_);
};
int JoyCon_L::Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) {
    return impl_->Rumblef(left, right, session_);
};

JoyCon_R::JoyCon_R(const Device &host) : JoyCon_R(new ControllerImpl(&host)){};
JoyCon_R::JoyCon_R(ControllerImpl *impl) : impl_(impl) {
    session_ = std::unique_ptr<Session>(impl_->OpenDevice(1, PID));
};
int JoyCon_R::Pair() { return impl_->Pair(session_); };
int JoyCon_R::Poll(PollType type) { return impl_->Poll(type, session_); };
int JoyCon_R::BackupMemory(Progress progress) { return impl_->BackupMemory(progress, session_); };
int JoyCon_R::RestoreMemory(Progress progress) { return impl_->RestoreMemory(progress, session_); };
int JoyCon_R::GetData(ControllerData &data) { return impl_->GetData(data, session_); };
int JoyCon_R::GetColor(ControllerColor &color) { return impl_->GetColor(color, session_); };
int JoyCon_R::SetColor(const ControllerColor &color) { return impl_->SetColor(color, session_); };
int JoyCon_R::SetLowPower(bool enable) { return impl_->SetLowPower(enable, session_); };
int JoyCon_R::SetPlayer(Player player, PlayerFlash flash) {
    return impl_->SetPlayer(player, flash, session_);
};
int JoyCon_R::SetImu(bool enable) { return impl_->SetImu(enable, session_); };
int JoyCon_R::SetRumble(bool enable) { return impl_->SetRumble(enable, session_); };
int JoyCon_R::Rumble(const rumble_data_t *left, const rumble_data_t *right) {
    return impl_->Rumble(nullptr, right, session_);
};
int JoyCon_R::Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) {
    return impl_->Rumblef(left, right, session_);
};
int JoyCon_R::SetMcuState(McuState state) { return impl_->SetMcuState(state, session_); };
int JoyCon_R::SetMcuMode(McuMode mode) { return impl_->SetMcuMode(mode, session_); };
int JoyCon_R::CheckMcuMode(McuMode mode) { return impl_->CheckMcuMode(mode, session_); };
int JoyCon_R::SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size, const HomeLightPattern *patterns) {
    return impl_->SetHomeLight(intensity, duration, repeat, size, patterns, session_);
};
int JoyCon_R::SetMcuNfcConfig() { return -1; };
int JoyCon_R::GetNfcNtag() { return -1; };
int JoyCon_R::GetNfcData() { return -1; };
int JoyCon_R::SetMcuIrConfig(const IrConfigFixed &fixed) {
    return impl_->SetMcuIrConfig(fixed, session_);
};
int JoyCon_R::SetMcuIrConfig(const IrConfigLive &live) {
    return impl_->SetMcuIrConfig(live, session_);
};
int JoyCon_R::CheckMcuIrMode(IrMode mode) { return impl_->CheckMcuIrMode(mode, session_); };
int JoyCon_R::SetMcuIrRegisters(const McuReg *regs, size_t size) {
    return impl_->SetMcuIrRegisters(regs, size, session_);
};
int JoyCon_R::SetIrConfig(const IrConfig &config, uint8_t *buffer, IrCallback cb) {
    return impl_->SetIrConfig(config, buffer, cb, session_);
};
int JoyCon_R::GetIrImage(const IrConfigFixed &fixed, uint8_t *buffer,
                         IrCallback cb) { return impl_->GetIrImage(fixed, buffer, cb, session_); };
int JoyCon_R::TestIR(int mode, uint8_t *buffer, IrCallback cb) {
    return impl_->TestIR(mode, buffer, cb, session_);
};

ProController::ProController(const Device &host) : ProController(new ControllerImpl(&host)){};
ProController::ProController(ControllerImpl *impl) : impl_(impl) { session_ = std::unique_ptr<Session>(impl_->OpenDevice(1, PID)); };
int ProController::Pair() { return impl_->Pair(session_); };
int ProController::Poll(PollType type) { return impl_->Poll(type, session_); };
int ProController::BackupMemory(Progress progress) {
    return impl_->BackupMemory(progress, session_);
};
int ProController::RestoreMemory(Progress progress) {
    return impl_->RestoreMemory(progress, session_);
};
int ProController::GetData(ControllerData &data) { return impl_->GetData(data, session_); };
int ProController::GetColor(ControllerColor &color) { return impl_->GetColor(color, session_); };
int ProController::SetColor(const ControllerColor &color) { return impl_->SetColor(color, session_); };
int ProController::SetLowPower(bool enable) { return impl_->SetLowPower(enable, session_); };
int ProController::SetPlayer(Player player, PlayerFlash flash) {
    return impl_->SetPlayer(player, flash, session_);
};
int ProController::SetImu(bool enable) { return impl_->SetImu(enable, session_); };
int ProController::SetRumble(bool enable) { return impl_->SetRumble(enable, session_); };
int ProController::Rumble(const rumble_data_t *left, const rumble_data_t *right) {
    return impl_->Rumble(left, right, session_);
};
int ProController::Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) {
    return impl_->Rumblef(left, right, session_);
};
int ProController::SetMcuState(McuState state) { return impl_->SetMcuState(state, session_); };
int ProController::SetMcuMode(McuMode mode) { return impl_->SetMcuMode(mode, session_); };
int ProController::CheckMcuMode(McuMode mode) { return impl_->CheckMcuMode(mode, session_); };
int ProController::SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size,
                                const HomeLightPattern *patterns) {
    return impl_->SetHomeLight(intensity, duration, repeat, size, patterns, session_);
};
int ProController::SetMcuNfcConfig() { return -1; };
int ProController::GetNfcNtag() { return -1; };
int ProController::GetNfcData() { return -1; };

JoyCon_Dual::JoyCon_Dual(const Device &host) : JoyCon_Dual(new ControllerImpl(&host)){};
JoyCon_Dual::JoyCon_Dual(ControllerImpl *impl) : impl_(impl) {
    session_l_ = std::unique_ptr<Session>(impl_->OpenDevice(1, JoyCon_L::PID));
    session_r_ = std::unique_ptr<Session>(impl_->OpenDevice(1, JoyCon_R::PID));
};
int JoyCon_Dual::Pair() { return impl_->Pair(session_l_, session_r_); };
int JoyCon_Dual::Poll(PollType type) { return impl_->Poll(type, session_l_, session_r_); };
int JoyCon_Dual::BackupMemory(Progress progress) {
    return impl_->BackupMemory(progress, session_l_, session_r_);
};
int JoyCon_Dual::RestoreMemory(Progress progress) {
    return impl_->RestoreMemory(progress, session_l_, session_r_);
};
int JoyCon_Dual::GetData(ControllerData &data) {
    return impl_->GetData(data, session_l_, session_r_);
};
int JoyCon_Dual::GetColor(ControllerColor &color) {
    return impl_->GetColor(color, session_l_, session_r_);
};
int JoyCon_Dual::SetColor(const ControllerColor &color) { return impl_->SetColor(color, session_l_, session_r_); };
int JoyCon_Dual::SetLowPower(bool enable) {
    return impl_->SetLowPower(enable, session_l_, session_r_);
};
int JoyCon_Dual::SetPlayer(Player player, PlayerFlash flash) {
    return impl_->SetPlayer(player, flash, session_l_, session_r_);
};
int JoyCon_Dual::SetImu(bool enable) { return impl_->SetImu(enable, session_l_, session_r_); };
int JoyCon_Dual::SetRumble(bool enable) {
    return impl_->SetRumble(enable, session_l_, session_r_);
};
int JoyCon_Dual::Rumble(const rumble_data_t *left, const rumble_data_t *right) {
    return impl_->Rumble(left, right, session_l_, session_r_);
};
int JoyCon_Dual::Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) {
    return impl_->Rumblef(left, right, session_l_, session_r_);
};
int JoyCon_Dual::SetMcuState(McuState state) { return impl_->SetMcuState(state, session_r_); };
int JoyCon_Dual::SetMcuMode(McuMode mode) { return impl_->SetMcuMode(mode, session_r_); };
int JoyCon_Dual::CheckMcuMode(McuMode mode) { return impl_->CheckMcuMode(mode, session_r_); };
int JoyCon_Dual::SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size,
                              const HomeLightPattern *patterns) {
    return impl_->SetHomeLight(intensity, duration, repeat, size, patterns, session_r_);
};

// C
extern "C" {
int calc_rumble_data(const rumble_data_f_t *rumblef, rumble_data_t *rumble) {
    return CalcRumblef(*rumble, rumblef->freq_h, rumblef->freq_h_amp, rumblef->freq_l,
                       rumblef->freq_l_amp);
}

void Controller_destroy(Controller *controller) { delete controller; }

int Controller_pair(Controller *controller) {
    return controller->Pair();
}

int Controller_poll(Controller *controller, poll_type_t type) {
    return controller->Poll(type);
}

int Controller_set_low_power(Controller *controller, int enable) {
    return controller->SetLowPower(enable & 0x1);
}

int Controller_set_player(Controller *controller, uint8_t player, uint8_t flash) {
    return controller->SetPlayer(Player_(player & 0xf), PlayerFlash_(flash & 0xf));
}

int Controller_set_rumble(Controller *controller, int enable) {
    return controller->SetRumble(enable & 0x1);
}

int Controller_rumble(Controller *controller, const rumble_data_t *left, const rumble_data_t *right) {
    return controller->Rumble(left, right);
}

int Controller_rumblef(Controller *controller, const rumble_data_f_t *left,
                       const rumble_data_f_t *right) {
    return controller->Rumblef(left, right);
}

int Controller_testIR(Controller *controller, int mode, uint8_t *image, int (*callback)()) {
    if (controller->category() == JOYCON_R)
        return static_cast<JoyCon_R *>(controller)->TestIR(mode, image, callback);
    return -EINVAL;
}
}