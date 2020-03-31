#!/usr/bin/python3

from ctypes import *
from tkinter import *
from PIL import Image, ImageTk
import sys
import numpy as np
import signal
import time
import threading
from scipy.fftpack import fft, ifft
from matplotlib import pyplot as plt
import hashlib

libjc = CDLL('build/libjoycon.so')

_quit = False


def quit(signum, frame):
    print('quit')
    global _quit
    _quit = True
    sys.exit()


if __name__ == '__main__':
    mode = 3
    modes = ((240, 320),(120, 160),(60, 80),(30, 40))

    shape = modes[mode]
    size = shape[0]*shape[1]
    buffer = bytes(size)

    root = Tk()
    label = Label(root)
    label.pack()

    def plot():
        global label
        img = np.asarray(tuple(buffer), dtype=np.uint8)
        img = np.reshape(img, shape)
        img = Image.fromarray(img)
        img = ImageTk.PhotoImage(img)
        label.image = img
        label.configure(image=img)

    @CFUNCTYPE(c_int)
    def callback():
        global _quit
        #print('callback : ', _quit)
        plot()
        return -1 if _quit else 0

    def test():
        global controller, buffer, callback
        libjc.Controller_testIR(controller, c_int(mode), c_char_p(buffer), callback)

    signal.signal(signal.SIGINT, quit)
    signal.signal(signal.SIGTERM, quit)

    # '''
    Controller_create = libjc.Controller_create
    Controller_create.restype = c_voidp
    controller = Controller_create(2)
    libjc.Controller_set_player(controller, 0, 4, None)
    t = threading.Thread(target=test)
    t.start()
    # t.join()
    root.mainloop()
    libjc.Controller_destroy(controller)
    # '''
