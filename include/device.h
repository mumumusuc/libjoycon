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

#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg)
#endif

#pragma pack(1)

typedef struct MacAddress {
    uint8_t _0;
    uint8_t _1;
    uint8_t _2;
    uint8_t _3;
    uint8_t _4;
    uint8_t _5;
} mac_address_t;
typedef mac_address_t mac_address_be_t;
typedef mac_address_t mac_address_le_t;
#define mac_address_be(raw) \
    (mac_address_t) { (raw)[5], (raw)[4], (raw)[3], (raw)[2], (raw)[1], (raw)[0] }
#define mac_address_le(raw) \
    (mac_address_t) { (raw)[0], (raw)[1], (raw)[2], (raw)[3], (raw)[4], (raw)[5] }
#define MAC_ADDRESS_INIT \
    { 0, 0, 0, 0, 0, 0 }
STATIC_ASSERT(sizeof(MacAddress) == 6, "sizeof MacAddress == 6");

typedef struct Alias {
    char raw[20];
} alias_t;
STATIC_ASSERT(sizeof(Alias) == 20, "sizeof Alias == 20");

typedef struct U16 {
    uint8_t _0;
    uint8_t _1;
} u16_t;
STATIC_ASSERT(sizeof(U16) == 2, "sizeof U16 == 2");
typedef u16_t u16_le_t;
typedef u16_t u16_be_t;
#define U16(raw) \
    { (raw)[0], (raw)[1] }
#define u16_as_le16(u16) ((u16)._0 & 0x00FF) | (((u16)._1 << 8) & 0xFF00)
#define u16_as_be16(u16) ((u16)._1 & 0x00FF) | (((u16)._0 << 8) & 0xFF00)
// uint8_t* -> uint16_t
#define le16(raw) u16_as_le16((u16_t)U16(raw))
#define be16(raw) u16_as_be16((u16_t)U16(raw))
// uint16_t -> U16
#define u16_le(u16) \
    { (u16) & 0xff, ((u16) >> 8) & 0xff }
#define u16_be(u16) \
    { ((u16) >> 8) & 0xff, (u16)&0xff }

typedef struct U32 {
    uint8_t _0;
    uint8_t _1;
    uint8_t _2;
    uint8_t _3;
} u32_t;
STATIC_ASSERT(sizeof(U32) == 4, "sizeof U32 == 4");
#define U32(raw) \
    { (raw)[0], (raw)[1], (raw)[2], (raw)[3] }
#define u32_as_le32(u32) ((u32)._0 & 0x000000FF) | (((u32)._1 << 8) & 0x0000FF00) | (((u32)._2 << 16) & 0x00FF0000) | (((u32)._3 << 24) & 0xFF000000)
#define u32_as_be32(u32) ((u32)._3 & 0x000000FF) | (((u32)._2 << 8) & 0x0000FF00) | (((u32)._1 << 16) & 0x00FF0000) | (((u32)._0 << 24) & 0xFF000000)
#define le32(raw) u32_as_le32((u32_t)U32(raw))
#define be32(raw) u32_as_be32((u32_t)U32(raw))

#pragma pack()

#define mac_address_fmt "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX"
#define str_to_mac_address_le(address)                                                          \
    ({                                                                                          \
        mac_address_t mac = {};                                                                 \
        sscanf(address, mac_address_fmt, &mac._5, &mac._4, &mac._3, &mac._2, &mac._1, &mac._0); \
        mac;                                                                                    \
    })
#define str_to_mac_address_be(address)                                                          \
    ({                                                                                          \
        mac_address_t mac = {};                                                                 \
        sscanf(address, mac_address_fmt, &mac._0, &mac._1, &mac._2, &mac._3, &mac._4, &mac._5); \
        mac;                                                                                    \
    })
#define mac_address_le_to_str(address)                                                                         \
    ({                                                                                                         \
        char mac[18] = {'\0'};                                                                                 \
        sprintf(mac, mac_address_fmt, address._5, address._4, address._3, address._2, address._1, address._0); \
        mac;                                                                                                   \
    })
#define mac_address_be_to_str(address)                                                                         \
    ({                                                                                                         \
        char mac[18] = {'\0'};                                                                                 \
        sprintf(mac, mac_address_fmt, address._0, address._1, address._2, address._3, address._4, address._5); \
        mac;                                                                                                   \
    })
#define alias(str)          \
    ({                      \
        alias_t a = {};     \
        strcpy(a.raw, str); \
        a;                  \
    })

typedef enum DeviceRole {
    UNKNOWN,
    CONSOLE,
    CONTROLLER,
} device_role_t;

typedef struct DeviceDesc {
    device_role_t role;
    const char *name;
    const char *mac_address;
    const char *serial_number;
} device_desc_t;

#ifdef __cplusplus
#include <functional>
using Sender = std::function<ssize_t(const void *, size_t)>;
using Recver = std::function<ssize_t(void *, size_t)>;
#else
typedef ssize_t (*Recver)(const device_t *, void *, size_t);
typedef ssize_t (*Sender)(const device_t *, const void *, size_t);
#endif

typedef struct DeviceFunc {
    Sender sender;
    Recver recver;
    size_t send_size;
    size_t recv_size;
} device_func_t;

typedef struct Device {
    device_desc_t desc;
    device_func_t func;
} device_t;

extern const device_desc_t sNintendoSwitch;
extern const device_desc_t sJoyCon_L;
extern const device_desc_t sJoyCon_R;
extern const device_desc_t sProController;

#endif