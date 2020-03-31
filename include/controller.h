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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "controller_defs.h"
#include "input_report.h"
#include "mcu.h"
#include "output_report.h"
#include "session2.h"

#ifdef __cplusplus
#include <functional>
#include <list>
#include <stdexcept>
namespace controller {

using Progress = std::function<void(size_t, size_t)>;
using Callback = std::function<void(int)>;
using IrCallback = std::function<int(void)>;
class Controller;
class ControllerImpl;
class ProController;
class JoyCon_L;
class JoyCon_R;
class JoyCon_Dual;

std::list<Controller *> OpenDevices() noexcept;
Controller *OpenDevice(Category) noexcept;

// this is a abstract class
class Controller {
  protected:
    explicit Controller(){};

  public:
    static const auto VID = 0x057e;
    virtual ~Controller(){};
    friend decltype(OpenDevice) OpenDevice;
    virtual Category category() const = 0;
    virtual int Pair() = 0;
    virtual int Poll(PollType type) = 0;
    virtual int BackupMemory(Progress progress) = 0;
    virtual int RestoreMemory(Progress progress) = 0;
    virtual int GetData(ControllerData &data) = 0;
    virtual int GetColor(ControllerColor &color) = 0;
    virtual int SetColor(const ControllerColor &color) = 0;
    virtual int SetPlayer(Player player, PlayerFlash flash) = 0;
    virtual int SetLowPower(bool enable) = 0;
    virtual int SetImu(bool enable) = 0;
    // rumble part
    virtual int SetRumble(bool enable) = 0;
    virtual int Rumble(const rumble_data_t *left, const rumble_data_t *right) = 0;
    virtual int Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) = 0;
};

class ControllerImpl {
  private:
    friend class ProController;
    friend class JoyCon_L;
    friend class JoyCon_R;
    friend class JoyCon_Dual;
    std::vector<std::future<session::Result>> results_;
    std::mutex sess_lock_;
    std::mutex output_lock_;
    OutputReport *output_;
    Device host_;

  protected:
    explicit ControllerImpl(const Device *);
    virtual session::Session *OpenDevice(unsigned int, ...) { return new session::Session(&host_.func); };
    template <typename... Args>
    void Transmit(unsigned, const void *, session::Inspector, const Args &...);
    int Await();
    template <typename... Args>
    int ReadMemory(uint32_t, uint8_t, void *, const Args &...);
    template <typename... Args>
    int WriteMemory(uint32_t, uint8_t, const void *, const Args &...);
    template <typename... Args>
    int Pair(const Args &...);
    template <typename... Args>
    int Poll(PollType, const Args &...);
    template <typename... Args>
    int BackupMemory(Progress, const Args &...);
    template <typename... Args>
    int RestoreMemory(Progress, const Args &...);
    template <typename... Args>
    int GetData(ControllerData &, const Args &...);
    template <typename... Args>
    int GetColor(ControllerColor &, const Args &...);
    template <typename... Args>
    int SetColor(const ControllerColor &, const Args &...);
    template <typename... Args>
    int SetPlayer(Player, PlayerFlash, const Args &...);
    template <typename... Args>
    int SetLowPower(bool, const Args &...);
    template <typename... Args>
    int SetElapsedTime(uint8_t, const Args &...);
    template <typename... Args>
    int SetImu(bool enable, const Args &...);
    template <typename... Args>
    int SetRumble(bool enable, const Args &...);
    template <typename... Args>
    int Rumble(const rumble_data_t *, const rumble_data_t *, const Args &...);
    template <typename... Args>
    int Rumblef(const rumble_data_f_t *, const rumble_data_f_t *, const Args &...);
    template <typename... Args>
    int SetMcuState(McuState, const Args &...);
    template <typename... Args>
    int SetMcuMode(McuMode, const Args &...);
    template <typename... Args>
    int CheckMcuMode(McuMode, const Args &...);
    template <typename... Args>
    int SetHomeLight(uint8_t, uint8_t, uint8_t, size_t, const HomeLightPattern *, const Args &...);
    template <typename... Args>
    int SetMcuNfcConfig(const Args &...);
    template <typename... Args>
    int GetNfcNtag(const Args &...);
    template <typename... Args>
    int GetNfcData(const Args &...);
    template <typename... Args>
    int SetMcuIrConfig(const IrConfigFixed &, const Args &...);
    template <typename... Args>
    int SetMcuIrConfig(const IrConfigLive &, const Args &...);
    template <typename... Args>
    int SetMcuIrConfig(const IrConfig &, const Args &...);
    template <typename... Args>
    int CheckMcuIrMode(IrMode, const Args &...);
    template <typename... Args>
    int SetMcuIrRegisters(const McuReg *, size_t, const Args &...);
    template <typename... Args>
    int SetIrConfig(const IrConfig &, uint8_t *, IrCallback, const Args &...);
    template <typename... Args>
    int GetIrImage(const IrConfigFixed &, uint8_t *, IrCallback, const Args &...);
    template <typename... Args>
    int TestIR(int, uint8_t *, IrCallback, const Args &...);

  public:
    ~ControllerImpl();
};

class JoyCon_L : public Controller {
  private:
    std::unique_ptr<ControllerImpl> impl_;
    std::unique_ptr<session::Session> session_;

  public:
    static const auto PID = 0x2006;
    explicit JoyCon_L(ControllerImpl *);
    explicit JoyCon_L(const Device &);
    Category category() const override { return JOYCON_L; };
    int Pair() override;
    int Poll(PollType type) override;
    int BackupMemory(Progress progress) override;
    int RestoreMemory(Progress progress) override;
    int GetData(ControllerData &data) override;
    int GetColor(ControllerColor &color) override;
    int SetColor(const ControllerColor &color) override;
    int SetLowPower(bool enable) override;
    int SetPlayer(Player player, PlayerFlash flash) override;
    int SetImu(bool enable) override;
    int SetRumble(bool enable) override;
    int Rumble(const rumble_data_t *left, const rumble_data_t *right) override;
    int Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) override;
};

class JoyCon_R : public Controller {
  private:
    std::unique_ptr<ControllerImpl> impl_;
    std::unique_ptr<session::Session> session_;

  public:
    static const auto PID = 0x2007;
    explicit JoyCon_R(ControllerImpl *);
    explicit JoyCon_R(const Device &);
    Category category() const override { return JOYCON_R; };
    int Pair() override;
    int Poll(PollType type) override;
    int BackupMemory(Progress progress) override;
    int RestoreMemory(Progress progress) override;
    int GetData(ControllerData &data) override;
    int GetColor(ControllerColor &color) override;
    int SetColor(const ControllerColor &color) override;
    int SetLowPower(bool enable) override;
    int SetPlayer(Player player, PlayerFlash flash) override;
    int SetImu(bool enable) override;
    int SetRumble(bool enable) override;
    int Rumble(const rumble_data_t *left, const rumble_data_t *right) override;
    int Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) override;
    int SetMcuState(McuState state);
    int SetMcuMode(McuMode mode);
    int CheckMcuMode(McuMode mode);
    int SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size, const HomeLightPattern *patterns);
    int SetMcuNfcConfig();
    int GetNfcNtag();
    int GetNfcData();
    int SetMcuIrConfig(const IrConfigFixed &fixed);
    int SetMcuIrConfig(const IrConfigLive &live);
    int CheckMcuIrMode(IrMode mode);
    int SetMcuIrRegisters(const McuReg *regs, size_t size);
    int SetIrConfig(const IrConfig &config, uint8_t *buffer, IrCallback cb);
    int GetIrImage(const IrConfigFixed &fixed, uint8_t *buffer, IrCallback cb);
    int TestIR(int, uint8_t *buffer, IrCallback cb);
};

class ProController : public Controller {
  private:
    std::unique_ptr<ControllerImpl> impl_;
    std::unique_ptr<session::Session> session_;

  public:
    static const auto PID = 0x2009;
    explicit ProController(ControllerImpl *);
    explicit ProController(const Device &);
    Category category() const override { return PRO_GRIP; };
    int Pair() override;
    int Poll(PollType type) override;
    int BackupMemory(Progress progress) override;
    int RestoreMemory(Progress progress) override;
    int GetData(ControllerData &data) override;
    int GetColor(ControllerColor &color) override;
    int SetColor(const ControllerColor &color) override;
    int SetLowPower(bool enable) override;
    int SetPlayer(Player player, PlayerFlash flash) override;
    int SetImu(bool enable) override;
    int SetRumble(bool enable) override;
    int Rumble(const rumble_data_t *left, const rumble_data_t *right) override;
    int Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) override;
    int SetMcuState(McuState state);
    int SetMcuMode(McuMode mode);
    int CheckMcuMode(McuMode mode);
    int SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size, const HomeLightPattern *patterns);
    int SetMcuNfcConfig();
    int GetNfcNtag();
    int GetNfcData();
};

class JoyCon_Dual : public Controller {
  private:
    std::unique_ptr<ControllerImpl> impl_;
    std::unique_ptr<session::Session> session_l_;
    std::unique_ptr<session::Session> session_r_;

  public:
    explicit JoyCon_Dual(ControllerImpl *);
    explicit JoyCon_Dual(const Device &);
    Category category() const override { return PRO_GRIP; };
    int Pair() override;
    int Poll(PollType type) override;
    int BackupMemory(Progress progress) override;
    int RestoreMemory(Progress progress) override;
    int GetData(ControllerData &data) override;
    int GetColor(ControllerColor &color) override;
    int SetColor(const ControllerColor &color) override;
    int SetLowPower(bool enable) override;
    int SetPlayer(Player player, PlayerFlash flash) override;
    int SetImu(bool enable) override;
    int SetRumble(bool enable) override;
    int Rumble(const rumble_data_t *left, const rumble_data_t *right) override;
    int Rumblef(const rumble_data_f_t *left, const rumble_data_f_t *right) override;
    int SetMcuState(McuState state);
    int SetMcuMode(McuMode mode);
    int CheckMcuMode(McuMode mode);
    int SetHomeLight(uint8_t intensity, uint8_t duration, uint8_t repeat, size_t size, const HomeLightPattern *patterns);
    /*
    int SetMcuNfcConfig() const;
    int GetNfcNtag() const;
    int GetNfcData() const;
    int SetMcuIrConfig(const IrConfigFixed &fixed) const;
    int SetMcuIrConfig(const IrConfigLive &live) const;
    int CheckMcuIrMode(IrMode mode) const;
    int SetMcuIrRegisters(const McuReg *regs, size_t size) const;
    int SetIrConfig(const IrConfig &config, uint8_t *buffer, Callback cb) const;
    int GetIrImage(const IrConfigFixed &fixed, uint8_t *buffer, Callback cb) const;
    int TestIR(uint8_t *buffer, Callback cb) const;
    */
};
} // namespace controller
#endif

typedef struct controller::Controller Controller;

#endif