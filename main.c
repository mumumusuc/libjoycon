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

#include "console.h"
#include "session.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("hello c\n");
    Console *console = Console_create();
    // Console_test(console,0);
    for (uint8_t i = 0; i <= 0xF; i++) {
        int ret = Console_setPlayer(console, i, 0);
        func_printf("console->setPlayer ret=%d", ret);
        usleep(200 * 1000);
    }
    Console_setPlayer(console, 0, 0xF);
    Console_destroy(console);
    return 0;
}