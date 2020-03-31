#include "console.h"
#include "controller.h"
#include "defs.h"
#include "report.h"
#include "session.h"
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void *input_thread(void *arg) {
    assert(arg);
    printf("enter input_thread\n");
    Console *console = (Console *)arg;
    ControllerData_t data;
    for (int i = 0; i < 300; i++) {
        bzero(&data, sizeof(data));
        int ret = console->getControllerData(&data);
        func_printf("getControllerData(%d)", ret);
        hex_dump("", &data, sizeof(data));
    }
    pthread_exit(NULL);
    return NULL;
}

int main() {
    std::cout << "hello c++" << std::endl;
    int ret = 0;
    Console *console = new Console();
    console->poll(POLL_STANDARD);
    // pthread_t input;
    // pthread_create(&input, NULL, input_thread, console);
    console->setHomeLight(0, 0x5, 0x3, sizeof(double_blink_pattern),
                          double_blink_pattern);

    typedef void (*CALLBACK)(int);
    CALLBACK callback = [](int result) {
        func_printf("callback -> %d", result);
    };
    int player = 0;
    int flash = 0;
    ret = console->backupFlashMemory(
        [&console, &callback, &player, &flash](size_t total, size_t current) {
            float progress = float(current) / (total)*100;
            func_printf("total : %ld, current : %ld, progress : %.2f%%", total,
                        current, progress);
            int _player = static_cast<int>(progress) / 25;
            int _flash = _player + 1;
            if (_player != player || _flash != flash) {
                player = _player;
                flash = _flash;
                int ret = console->setPlayer(
                    Player_t(player), static_cast<PlayerFlash_t>(0x1 << flash),
                    callback);
                func_printf("console->setPlayer -> %d", ret);
            }
        });
    func_printf("backupFlashMemory(%d) -> %s", ret, strerror(-ret));

    ControllerColor_t color = {};
    ret = console->getControllerColor(&color);
    func_printf("getControllerColor(%d)", ret);
    hex_dump("COLOR", (uint8_t *)&color, sizeof(color));
    for (uint8_t i = 0; i <= 0xF; i++) {
        int ret = console->setPlayer(static_cast<Player_t>(i),
                                     static_cast<PlayerFlash_t>(0), NULL);
        func_printf("console->setPlayer ret=%d", ret);
        usleep(100 * 1000);
    }
    console->setPlayer(static_cast<Player_t>(0),
                       static_cast<PlayerFlash_t>(0xF), NULL);
    // pthread_join(input, NULL);
    delete console;

    return ret;
}