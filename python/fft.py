#!/usr/bin/python3

from time import sleep
from libjoycon import *
from ctypes import *

if __name__ == '__main__':
    libjc = CDLL('../build/libjoycon.so')
    #'''
    calc_rumble_data = libjc.calc_rumble_data
    with open('xwzh.pcm', 'rb') as pcm:
        tune = Tune(pcm.read(), 2, 44100)
    rd_0, rd_1 = Rumble(0, 0, 0, 0), Rumble(0, 0, 0, 0)
    gain = 10
    with open('xwzh.jcm', 'wb') as f:
        f.write(b'\x02')  # channel
        it_0 = tune.rumblef(0, 32, gain)
        it_1 = tune.rumblef(1, 32, gain)
        try:
            while True:
                rf_0 = next(it_0)
                rf_1 = next(it_1)
                calc_rumble_data(byref(rf_0), byref(rd_0))
                calc_rumble_data(byref(rf_1), byref(rd_1))
                f.write(rd_0.raw())
                f.write(rd_1.raw())
        except StopIteration as e:
            pass
        f.flush()
    '''
    Controller_create = libjc.Controller_create
    Controller_create.restype = c_voidp
    controller = Controller_create(0)
    libjc.Controller_set_player(controller, 1, 0)
    libjc.Controller_set_rumble(controller, True)
    Controller_rumble = libjc.Controller_rumble
    with open('xwzh.jcm', 'rb') as f:
        raw = f.read()
        ch = raw[0]
        cursor = 1
        while cursor < len(raw):
            rl = raw[cursor:cursor+4]
            rr = raw[cursor+4:cursor+8]
            cursor += 8
            Controller_rumble(
                controller,
                byref(Rumble(rl[0], rl[1], rl[2], rl[3])),
                byref(Rumble(rr[0], rr[1], rr[2], rr[3]))
            )
            sleep(0.032)
    libjc.Controller_destroy(controller)
    #'''
