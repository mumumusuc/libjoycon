#ifndef CONSOLE_H
#define CONSOLE_H

#include "input_report.h"
#include "output_report.h"
#include "session.h"
#include <stdio.h>

#ifdef __cplusplus
typedef std::function<void(size_t, size_t)> Progress;
class Console {
  private:
    Session *_session;
    OutputReport *_output;
    int ReadFlashMemory(uint32_t, uint8_t, void *) const;
    int WriteFlashMemory(uint32_t, uint8_t, const uint8_t *) const;
    int CalcRumblef(rumble_t *, float, float, float, float) const;

  public:
    Console();
    ~Console();
    int Poll(PollType) const;
    int BackupFlashMemory(Progress) const;
    int RestoreFlashMemory(Progress) const;
    int GetControllerData(ControllerData *) const;
    int GetControllerColor(ControllerColor *) const;
    int SetPlayer(Player, PlayerFlash, void (*)(int)) const;
    int SetHomeLight(uint8_t, uint8_t, uint8_t, size_t,
                     const HomeLightPattern *) const;
    int Rumble(bool) const;
    int Rumble(const RumbleData *) const;
    int Rumblef(float, float, float, float, float, float, float, float) const;

    int SetMcuState(McuState) const;
    int SetMcuMode(McuMode) const;
    int CheckMcuMode(McuMode) const;

    int SetMcuIrConfig(const IrConfigFixed *) const;
    int SetMcuIrConfig(const IrConfigLive *) const;
    int SetMcuIrConfig(const IrConfig *) const;
    int CheckMcuIrMode(uint8_t) const;

    int SetMcuIrRegisters(const McuReg *, size_t) const;
    int SetIrConfig(const IrConfig *, uint8_t *, void (*callback)()) const;
    int GetIrImage(const IrConfigFixed *, uint8_t *, void (*callback)()) const;
    int TestIR(uint8_t *, void (*callback)()) const;

    int SetMcuNfcConfig() const;
    int GetNfcNtag() const;
    int GetNfcData() const;
};
extern "C" {
#endif

typedef struct Console Console;
Console *Console_create();
void Console_destroy(Console *);
int Console_test(Console *, uint8_t);
int Console_test_ir(Console *, uint8_t *, void (*callback)());
int Console_rumble_enable(Console *, int);
int Console_rumblef(Console *, float, float, float, float, float, float, float,
                    float);
int Console_set_player(Console *, uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif