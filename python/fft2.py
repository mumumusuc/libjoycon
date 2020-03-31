#!/usr/bin/python3

import numpy as np
import math
from time import sleep
from datetime import datetime
from ctypes import *
from matplotlib import pyplot as plt


def clamp(x, a, b):
    x = a if x < a else x
    x = b if x > b else x
    return x


'''
uint8_t freq_h;
    uint8_t freq_h_amp;
    uint8_t freq_l;
    uint8_t freq_l_amp;
'''


class Rumble(Structure):
    _fields_ = [
        ('freq_h', c_uint8),
        ('freq_h_amp', c_uint8),
        ('freq_l', c_uint8),
        ('freq_l_amp', c_uint8),
    ]

    def __init__(self, freq_h: int, freq_h_amp: int, freq_l: int, freq_l_amp: int):
        super().__init__(**{
            'freq_h': c_uint8(freq_h),
            'freq_h_amp': c_uint8(freq_h_amp),
            'freq_l': c_uint8(freq_l),
            'freq_l_amp': c_uint8(freq_l_amp),
        })


class Rumblef(Structure):
    _fields_ = [
        ('freq_h', c_float),
        ('freq_h_amp', c_float),
        ('freq_l', c_float),
        ('freq_l_amp', c_float),
    ]

    def __init__(self, freq_h: float, freq_h_amp: float, freq_l: float, freq_l_amp: float):
        super().__init__(**{
            'freq_h': c_float(freq_h),
            'freq_h_amp': c_float(freq_h_amp),
            'freq_l': c_float(freq_l),
            'freq_l_amp': c_float(freq_l_amp),
        })


class Tune:
    def __init__(self, raw: bytes, channels: int, sampler: int, period: float):
        if channels not in [1, 2]:
            raise ValueError('channels must be 1 or 2')
        self._channels = channels
        self._sampler = sampler
        sample_period = 1.0/self._sampler
        sample_size = int(period*sampler/1000)
        self._period = sample_size * sample_period
        self._times = np.arange(sample_size) * sample_period
        self._freq_base = 1.0 / self._period
        self._freq_ceil = 1250.0 * self._period
        self._frequencies = np.arange(int(sample_size/2))*freq_base
        '''
        self._data_len = int(len(raw) / channels)
        self._data = {}
        if self._channels == 1:
            self._data = np.array([0] * self._data_len, dtype=np.int16)
        else:
            self._data_l = np.array([0] * self._data_len, dtype=np.int16)
            self._data_r = np.array([0] * self._data_len, dtype=np.int16)

        for i in range(int(self._data_len / self._channels)):
            if self._channels == 1:
                self._data[i] = (
                    raw[2 * i + 0] & 0xff) | ((raw[2 * i + 1] << 8) & 0xff00)
            else:
                self._data_l[i] = (
                    raw[4 * i + 0] & 0xff) | ((raw[4 * i + 1] << 8) & 0xff00)
                self._data_r[i] = (
                    raw[4 * i + 2] & 0xff) | ((raw[4 * i + 3] << 8) & 0xff00)
        # 32768
        if self._channels == 1:
            A = abs(self._data).max()
            self._data = self._data.astype('float32') / A
        else:
            A = max(abs(self._data_l).max(), abs(self._data_r).max())
            self._data_l = self._data_l.astype('float32') / A
            self._data_r = self._data_r.astype('float32') / A
        '''
    @staticmethod
    def _clamp(freq, amp):
        if 120 < freq < 280:
            return clamp(amp, 0, 0.2)
        return clamp(amp, 0, 1.0)

    def fft(self, channel: int, sampler: int):
        if channel < 0 or channel > self._channels - 1:
            raise ValueError('request channel invalid.')
        data = getattr(self, 'channel_%d' % channel)
        total = int(self._data_len / sampler)
        i = 0
        while i < total:
            y = data[i * sampler:(i + 1) * sampler]
            yy = np.fft.fft(y)
            yy = abs(yy) / sampler
            i += 1
            yield y, yy[:int(sampler / 2)] * 2
        return 0

    def rumble(self, yy, base_freq: float):
        srt = np.argsort(-yy[:int(1250 / base_freq)])
        f_h, f_l, a_h, a_l = 0, 0, 0, 0
        for i in range(len(srt)):
            if f_h > 0 and f_l > 0:
                break
            k, v = f[srt[i]], yy[srt[i]] * 3
            if k > 640:
                if f_h == 0:
                    f_h, a_h = k, self._clamp(k, v)
            elif k > 40:
                if f_l == 0:
                    f_l, a_l = k, self._clamp(k, v)
        print('[{}, {}],[{}, {}]'.format(f_h, a_h, f_l, a_l))
        return Rumblef(f_h, a_h, f_l, a_l)

    def write_to(self, file, n):
        pass

    def __getattr__(self, name):
        if name == 'channel_0':
            if self._channels == 1:
                return self._data
            else:
                return self._data_l
        elif name == 'channel_1' and self._channels == 2:
            return self._data_r


def write_to(Tune tune, file: str):
    calc_rumble_data = libjc.calc_rumble_data
    with open(file, 'wb') as f:
        f.write(channels)
        rd_0, rd_1 = Rumble(0, 0, 0, 0), Rumble(0, 0, 0, 0)
        g_yy_0 = tune.fft(0, n)
        g_yy_1 = tune.fft(1, n)
        try:
            while True:
                _, yy_0 = next(g_yy_0)
                _, yy_1 = next(g_yy_1)
                rf_0 = tune.rumble(yy_0, base_freq)
                rf_1 = tune.rumble(yy_1, base_freq)
                calc_rumble_data(byref(rf_0), byref(rd_0))
                calc_rumble_data(byref(rf_1), byref(rd_1))
                f.write(rd_0.freq_h, rd_0.freq_h_amp,
                        rd_0.freq_l, rd_0.freq_l_amp)
                f.write(rd_1.freq_h, rd_1.freq_h_amp,
                        rd_1.freq_l, rd_1.freq_l_amp)
        except StopIteration as e:
            pass
        f.flush()


if __name__ == '__main__':
    '''
    libjc = CDLL('build/libjoycon.so')
    Controller_create = libjc.Controller_create
    Controller_create.restype = c_voidp
    controller = Controller_create(3)
    #libjc.Controller_poll(controller, 0x30)
    libjc.Controller_set_player(controller, 1, 0)
    libjc.Controller_set_rumble(controller, True)
    Controller_rumblef = libjc.Controller_rumblef
    # '''
    Fs = 44100
    Ts = 1 / Fs
    Tt = 32 / 1000.0
    t = np.arange(0, Tt, Ts)
    n = len(t)
    k = np.arange(n)
    T = n / Fs
    f = k / T  # k/n*Fs
    f_max_i = 1250 / f[1]
    f = f[range(int(n / 2))]
    print(n, f[1])

    data = {}
    fig, ax = plt.subplots(2, 2, sharey='row')
    tune = None
    # '''
    with open('xwzh.pcm', 'rb') as pcm:
        tune = Tune(pcm.read(), 2, 44100)
    g_yy_0 = tune.fft(0, n)
    g_yy_1 = tune.fft(1, n)
    try:
        while True:
            begin = datetime.now()
            y, yy = next(g_yy_0)
            y_1, yy_1 = next(g_yy_1)
            end = datetime.now()
            print('fft x2 use {} ms'.format((end - begin).microseconds / 1000))
            '''
            ax[0][0].cla()
            ax[0][0].plot(t, y)
            ax[0][0].set_xlabel('time')
            ax[0][0].set_ylabel('amp')
            ax[0][0].set_ylim(-1, 1)
            ax[0][1].cla()
            ax[0][1].plot(t, y_1)
            ax[1][0].cla()
            ax[1][0].plot(f, yy, 'r')
            ax[1][0].set_xlabel('Hz')
            ax[1][0].set_ylabel('v')
            ax[1][0].set_ylim(0, 1)
            ax[1][1].cla()
            ax[1][1].plot(f, yy_1)
            plt.pause(0.01)
            '''
            '''
            Controller_rumblef(
                controller,
                byref(tune.rumble(yy, f[1])),
                byref(tune.rumble(yy_1, f[1])),
            )
            sleep(Tt)
            '''
    except StopIteration as e:
        pass
    # plt.show()
    # libjc.Controller_destroy(controller)
