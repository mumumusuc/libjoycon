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

#ifndef LIBJOYCON_H
#define LIBJOYCON_H

// export C apis
#ifdef __cplusplus
extern "C" {
#endif
#include "controller.h"

extern int calc_rumble_data(const rumble_data_f_t *, rumble_data_t *);

// controller
extern Controller *Controller_create(category_t);
extern void Controller_destroy(Controller *);
extern int Controller_pair(Controller *);
extern int Controller_poll(Controller *, poll_type_t);
extern int Controller_set_low_power(Controller *, int);
extern int Controller_set_player(Controller *, uint8_t, uint8_t);
extern int Controller_set_rumble(Controller *, int);
extern int Controller_rumble(Controller *, const rumble_data_t *, const rumble_data_t *);
extern int Controller_rumblef(Controller *, const rumble_data_f_t *, const rumble_data_f_t *);
extern int Controller_testIR(Controller *, int, uint8_t *, int (*)());

// console

#ifdef __cplusplus
}
#endif

#endif