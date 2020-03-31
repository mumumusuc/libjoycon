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

#ifndef LOG_H
#define LOG_H

#if __debug__
#include <stdio.h>
#if __android__
#include <android/log.h>
#define __log_d(tag, fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#include <time.h>
#define __file stdout
#define __log_d(tag, fmt, ...) fprintf(__file, "(%ld)[%s]" fmt "\n", clock(), (tag), ##__VA_ARGS__)
#endif

#ifdef __cplusplus
static inline void __hex_d(const char *tag, const void *buf, size_t len) {
    char *src = new char[3 * len]();
    char *tmp = src;
    const unsigned char *dst = reinterpret_cast<const unsigned char *>(buf);
    for (int i = 0; i < len - 1; ++i) {
        sprintf(tmp, "%02hhx ", dst[i]);
        tmp += 3;
    }
    sprintf(tmp, "%02hhx", dst[len - 1]);
    __log_d(tag, "%s", src);
    delete[] src;
}
#else
#define __hex_d(tag, buf, len) \
    do {                       \
    } while (0)
#endif

#define log_d(tag, fmt, ...) __log_d(tag, "" fmt, ##__VA_ARGS__)
#define hex_d(tag, buf, len) __hex_d(tag, buf, len)
#else
#define log_d(tag, fmt, ...)
#define hex_d(tag, buf, len)
#endif

#endif