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
#include "log.h"
#include "session2.h"
#include <assert.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>

#define msleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds((ms)))

std::future<int> test_future() {
    std::promise<int> p;
    auto f = p.get_future();
    p.set_value(10);
    return f;
}

static int test_l() {
    int ret = 0;
    try {
        auto jc = new controller::JoyCon_L(nullptr);
        //jc->Poll(POLL_STANDARD);
        for (uint8_t j = 0; j < 0xff; j++)
            for (uint8_t i = 0; i <= 0xf; ++i) {
                ret = jc->SetPlayer(static_cast<Player>(i), PLAYER_FLASH_0);
                msleep(32);
            }
        ret = jc->SetPlayer(PLAYER_0, PLAYER_FLASH_4);
        delete jc;
    } catch (std::exception &e) {
        log_d(__func__, "%s", e.what());
    }
    return ret;
}

static int test_r() {
    int ret = 0;
    try {
        auto jc = new controller::JoyCon_R(nullptr);
        //jc->Poll(POLL_STANDARD);
        for (uint8_t j = 0; j < 0xf; j++)
            for (uint8_t i = 0; i <= 0xf; ++i) {
                ret = jc->SetPlayer(static_cast<Player>(i), PLAYER_FLASH_0);
                msleep(32);
            }
        ret = jc->SetPlayer(PLAYER_0, PLAYER_FLASH_4);
        delete jc;
    } catch (std::exception &e) {
        log_d(__func__, "%s", e.what());
    }
    return ret;
}

static mac_address_le_t ns_mac = {0x62, 0x9a, 0x15, 0xeb, 0x68, 0xdc};

static int test_session() {
    int ret = 0;
    DeviceFunc dev_fun = {
        .sender = [](const void *buffer, size_t size) -> ssize_t {
            hex_d("SEND", buffer, size);
            return size;
        },
        .recver = [](void *buffer, size_t size) -> ssize_t {
            strcpy(reinterpret_cast<char *>(buffer), "test_session");
            return size;
        },
        .send_size = OUTPUT_REPORT_SIZE,
        .recv_size = INPUT_REPORT_STAND_SIZE,
    };

    session::Session sess(&dev_fun);
    uint8_t buffer[OUTPUT_REPORT_SIZE] = "sess test";

    auto f1 = std::async(
        [&sess, buffer]() {
            for (int i = 0; i < 10; i++) {
                log_d(__func__, "time test %d -----ing", i);
                auto f = sess.Transmit(
                    5, buffer, [](const void *input) {
                        //hex_dump("RECV", input, INPUT_PACKET_STAND_SIZE);
                        return session::WAITING;
                    });
                assert(f.get() == session::TIMEDOUT);
                log_d(__func__, "time test %d -----OK", i);
            }
            log_d(__func__, "time test over");
            return 0;
        });

    auto f2 = std::async(
        [&sess, buffer]() {
            for (int i = 0; i < 10; i++) {
                log_d(__func__, "done test %d -----ing", i);
                auto f = sess.Transmit(
                    5, buffer, [](const void *input) {
                        //hex_dump("RECV", input, INPUT_PACKET_STAND_SIZE);
                        return session::DONE;
                    });
                assert(f.get() == session::DONE);
                log_d(__func__, "done test %d -----OK", i);
            }
            log_d(__func__, "done test over");
            return 0;
        });

    ret = f1.get();
    ret = f2.get();
    return ret;
}

static int test_dual() {
    int ret = 0;
    auto jc = controller::JoyCon_Dual(nullptr);
    jc.Pair();
    jc.SetLowPower(false);
    //jc.SetMcuMode(McuMode(0));
    jc.SetImu(true);
    for (uint8_t j = 0; j < 0x7f; j++) {
        //jc.Poll(POLL_STANDARD);
        for (uint8_t i = 0; i <= 0xf; ++i) {
            ret = jc.SetPlayer(static_cast<Player>(i), PLAYER_FLASH_0);
            msleep(32);
        }
    }
    ret = jc.SetPlayer(PLAYER_0, PLAYER_FLASH_4);
    return ret;
}

int main() {
    std::cout << "hello test c++" << std::endl;
    int ret = 0;
    ret = test_session();
    return ret;
}

// valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --undef-value-errors=no  -s ./build/test_cpp