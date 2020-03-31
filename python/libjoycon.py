#!/usr/bin/python3

import numpy as np
from ctypes import *
from matplotlib import pyplot as plt


def clamp(x, a, b):
    x = a if x < a else x
    x = b if x > b else x
    return x


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

    def raw(self):
        return bytearray([self.freq_h, self.freq_h_amp, self.freq_l, self.freq_l_amp])


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
    def __init__(self, raw: bytes, channel: int, sampler: int):
        if channel not in [1, 2]:
            raise ValueError('channel must be 1 or 2')
        self._channel = channel
        self._sampler = sampler
        '''
        sample_period = 1.0/self._sampler
        sample_size = int(self._sampler*period/1000)
        self._period = sample_size / self._sampler
        self._times = np.arange(sample_size) * sample_period
        self._freq_base = self._sampler / sample_size
        self._freq_ceil = int(1250 * self._period)
        self._freqs = np.arange(int(sample_size/2))*self._freq_base
        '''
        self._data = {}
        data_len = int(len(raw) / self._channel)
        for i in range(self._channel):
            self._data['%d' % i] = np.array([0] * data_len, dtype=np.int16)
        index, cursor = 0, 0
        while cursor < len(raw):
            for i in range(self._channel):
                value = (raw[cursor] & 0xff) | ((raw[cursor+1] << 8) & 0xff00)
                self._data['%d' % i][index] = value
                cursor += 2
            index += 1
        # 32768
        _max = 0
        for i in range(self._channel):
            _max = max(abs(self._data['%d' % i]).max(), _max)
        for i in range(self._channel):
            self._data['%d' % i] = self._data['%d' % i].astype('float32')/_max

    def __getattr__(self, name):
        if name == 'period':
            return self._period
        if name == 'times':
            return self._times
        if name == 'freqs':
            return self._freqs

    def __str__(self):
        return 'channel : {}\nsampler : {}\nperiod : {}\nfreq_base : {}\ntimes : {}\nfreqs : {}'.format(self._channel, self._sampler, self._period, self._freq_base, self._times, self._freqs)

    def fft(self, chan_no: int, period: float):
        '''
        chan_no: channel data
        period: millisecond
        '''
        if chan_no < 0 or chan_no > self._channel - 1:
            raise ValueError('request channel invalid.')
        data = self._data['%d' % chan_no]
        size = len(data)
        _count = int(self._sampler*period/1000)
        _period = _count / self._sampler
        _times = np.arange(_count) / self._sampler
        _base = self._sampler / _count
        _freqs = np.arange(int(_count/2))*_base
        cursor = 0
        while cursor < size:
            y = data[cursor:cursor+_count]
            yy = np.fft.fft(y)
            yy = abs(yy) / _count
            yield _times, y, _freqs, yy[:int(_count / 2)] * 2
            cursor += _count
        return 0

    def rumblef(self, chan_no: int, period: float, gain: float):
        '''
        80 < f_h < 1252
        40 < f_l < 626
        '''
        it = self.fft(chan_no, period)
        floor = int(40 * period/1000)
        ceil = int(1252 * period/1000)
        try:
            while True:
                _, _, xx, yy = next(it)
                sort = np.argsort(-yy[floor:ceil])
                f_h, f_l, a_h, a_l = 0, 0, 0, 0
                for i in range(len(sort)):
                    if f_h > 0 and f_l > 0:
                        break
                    k, v = xx[sort[i]], yy[sort[i]]
                    if k < 40 or k > 1252:
                        continue
                    if k > 626:
                        if f_h == 0:
                            f_h, a_h = k, v
                    elif f_l == 0:
                        f_l, a_l = k, v
                    '''
                    elif k < 80:
                        if f_l == 0:
                            f_l, a_l = k, v
                    elif f_h == 0:
                        f_h, a_h = k, v
                    else:
                        f_l, a_l = k, v
                    '''
                #print('[{}, {}],[{}, {}]'.format(f_h, a_h, f_l, a_l))
                f_h, a_h = Tune._clamp(f_h, a_h, gain, 320)
                f_l, a_l = Tune._clamp(f_l, a_l, gain, 160)
                yield Rumblef(f_h, a_h, f_l, a_l)
        except StopIteration:
            pass
        return None

    @classmethod
    def _clamp(cls, f: float, a: float, gain: float, default: float):
        if f == 0:
            return default, 0
        if 160 <= f <= 360:
            return f, clamp(a*gain, 0, 0.1)
        return f, clamp(a*gain, 0, 1.0)


if __name__ == '__main__':
    fig, ax = plt.subplots(2, 1, sharey='row')
    with open('xwzh.pcm', 'rb') as pcm:
        tune = Tune(pcm.read(), 2, 44100)
    it = tune.fft(0, 32)
    try:
        while True:
            x, y, xx, yy = next(it)
            ax[0].cla()
            ax[0].plot(x, y)
            ax[0].set_xlabel('Time')
            ax[0].set_ylabel('A')
            ax[0].set_ylim(-1, 1)
            ax[1].cla()
            ax[1].vlines(x=xx, ymin=0, ymax=yy, color='g')
            ax[1].scatter(x=xx, y=yy, s=5, color='g')
            ax[1].set_xlabel('Hz')
            ax[1].set_ylabel('V')
            ax[1].set_ylim(0, 1)
            plt.pause(0.01)
    except StopIteration:
        pass
    plt.show()
