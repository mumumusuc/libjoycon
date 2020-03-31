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

//
// Created by mumumusuc on 20-1-18.
//

#include "controller.h"
#include "log.h"
#include <cassert>
#include <errno.h>
#include <jni.h>
#include <memory>
#include <unistd.h>

#define CLASS_NAME "com/mumumusuc/libjoycon/Controller"
#define NELEM(m) (sizeof((m)) / sizeof(JNINativeMethod))
#define debug(fmt, ...) log_d(__func__, fmt, ##__VA_ARGS__)

using namespace controller;

static JavaVM *g_vm = nullptr;
static jobject g_obj;
static jmethodID method_setReport;
static jmethodID method_sendData;
static const char _map[] = "0123456789ABCDEF";

static jlong create(JNIEnv *, jobject, jint);
static void destroy(JNIEnv *, jobject, jlong);
static jint poll(JNIEnv *, jobject, jlong, jbyte);
static jint set_player(JNIEnv *, jobject, jlong, jbyte, jbyte);
static jint set_rumble(JNIEnv *, jobject, jlong, jboolean);
static jint rumble(JNIEnv *, jobject, long, jshort, jbyte, jbyte, jbyte, jbyte, jbyte, jbyte, jbyte);
static jint rumblef(JNIEnv *, jobject, jlong, jfloat, jfloat, jfloat, jfloat, jfloat, jfloat, jfloat, jfloat);
static void classInitNative(JNIEnv *env, jclass clazz) {
    method_setReport = env->GetMethodID(clazz, "setReport", "(Ljava/lang/String;)V");
    method_sendData = env->GetMethodID(clazz, "sendData", "(Ljava/lang/String;)V");
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void *)classInitNative},
    {"create", "(I)J", (void *)create},
    {"destroy", "(J)V", (void *)destroy},
    {"poll", "(JB)I", (void *)poll},
    {"set_player", "(JBB)I", (void *)set_player},
    {"set_rumble", "(JZ)I", (void *)set_rumble},
    {"rumble", "(JBBBBBBBB)I", (void *)rumble},
    {"rumblef", "(JFFFFFFFF)I", (void *)rumblef},
};

static inline int jniRegisterNativeMethod(JNIEnv *env, const char *class_name, const JNINativeMethod *methods, int size) {
    int ret = 0;
    jclass clazz = nullptr;
    clazz = env->FindClass(class_name);
    if (clazz == nullptr)
        return -1;
    ret = env->RegisterNatives(clazz, methods, size);
    env->DeleteLocalRef(clazz);
    return ret;
}

jint JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *env = nullptr;
    if (jvm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;
    jniRegisterNativeMethod(env, CLASS_NAME, sMethods, NELEM(sMethods));
    return JNI_VERSION_1_6;
}

static void byte_to_string(char *str, const uint8_t *bytes, size_t len) {
    for (unsigned i = 0; i < len; ++i) {
        str[2 * i + 0] = _map[(bytes[i] >> 4) & 0xf];
        str[2 * i + 1] = _map[bytes[i] & 0xf];
    }
    str[2 * len] = '\0';
}

static ssize_t send(const void *buffer, size_t len) {
    int ret = 0;
    JNIEnv *env = nullptr;
    jstring report = nullptr;
    bool need_detach = false;
    ret = g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED) {
        ret = g_vm->AttachCurrentThread(&env, nullptr);
        if (ret != JNI_OK) {
            debug("attach current thread to JVM error: %d", ret);
            return ret;
        }
        need_detach = true;
    }
    auto str = (char *)calloc(1, len * 2 + 1);
    byte_to_string(str, reinterpret_cast<const uint8_t *>(buffer), len);
    report = env->NewStringUTF(str);
    env->CallVoidMethod(g_obj, method_setReport, report);
    env->DeleteLocalRef(report);
    free(str);
    if (need_detach) {
        g_vm->DetachCurrentThread();
    }
    env = nullptr;
    return OUTPUT_REPORT_SIZE;
}

static ssize_t recv(void *buffer, size_t len) {
    //debug("%s not implemented", __func__);
    usleep(1000 * 16);
    return 0;
}

static const Device sDevice = {
    .desc = sNintendoSwitch,
    .func = {
        .sender = send,
        .recver = recv,
        .send_size = OUTPUT_REPORT_SIZE,
        .recv_size = INPUT_REPORT_STAND_SIZE,
    },
};

static jlong create(JNIEnv *env, jobject object, jint category) {
    Controller *handle = nullptr;
    if (category < PRO_GRIP || category > JOYCON) {
        debug("cannot create controller, [%d] is not in Category", category);
        goto done;
    }
    env->GetJavaVM(&g_vm);
    g_obj = env->NewGlobalRef(object);
    switch (category) {
    case PRO_GRIP:
        handle = new ProController(sDevice);
    case JOYCON_L:
        handle = new JoyCon_L(sDevice);
    case JOYCON_R:
        handle = new JoyCon_R(sDevice);
    case JOYCON:
        handle = new JoyCon_Dual(sDevice);
    default:
        break;
    }
done:
    return reinterpret_cast<jlong>(handle);
}

static void destroy(JNIEnv *env, jobject object, jlong handle) {
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    delete controller;
    env->DeleteGlobalRef(g_obj);
    g_vm = nullptr;
}

static jint poll(JNIEnv *env, jobject object, jlong handle, jbyte type) {
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    return controller->Poll(PollType(type));
}

static jint set_player(JNIEnv *env, jobject object, jlong handle, jbyte player, jbyte flash) {
    debug();
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    return controller->SetPlayer(Player(player), PlayerFlash(flash));
}

static jint set_rumble(JNIEnv *env, jobject object, jlong handle, jboolean enable) {
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    return controller->SetRumble(enable);
}

static jint
rumble(JNIEnv *env, jobject object, long handle,
       jshort hf_l, jbyte hfa_l, jbyte lf_l, jbyte lfa_l, jbyte hf_r, jbyte hfa_r, jbyte lf_r,
       jbyte lfa_r) {
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    auto l = RumbleData{(uint8_t)hf_l, (uint8_t)hfa_l, (uint8_t)lf_l, (uint8_t)lfa_l};
    auto r = RumbleData{(uint8_t)hf_r, (uint8_t)hfa_r, (uint8_t)lf_r, (uint8_t)lfa_r};
    return controller->Rumble(&l, &r);
}

static jint
rumblef(JNIEnv *env, jobject object, jlong handle,
        jfloat hf_l, jfloat hfa_l, jfloat lf_l, jfloat lfa_l,
        jfloat hf_r, jfloat hfa_r, jfloat lf_r, jfloat lfa_r) {
    auto controller = reinterpret_cast<controller::Controller *>(handle);
    auto l = RumbleDataF{hf_l, hfa_l, lf_l, lfa_l};
    auto r = RumbleDataF{hf_r, hfa_r, lf_r, lfa_r};
    return controller->Rumblef(&l, &r);
}
