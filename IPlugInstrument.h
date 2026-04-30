#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "RobinPanelControl.h"
#include "RobinKeyboardControl.h"

const int kNumPresets = 1;

enum EParams
{
  kParamMode = 0,
  kParamVelMode,
  kParamSustain,
  kNumParams
};

#if IPLUG_DSP
#include "IPlugInstrument_DSP.h"
#endif

enum EControlTags
{
  kCtrlTagKeyboard = 0,
  kCtrlTagPanel,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class IPlugInstrument final : public Plugin
{
public:
  IPlugInstrument(const InstanceInfo& info);

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  void OnUIOpen() override;
  void OnMidiMsgUI(const IMidiMsg& msg) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;
  bool SerializeState(IByteChunk& chunk) const override;
  int  UnserializeState(const IByteChunk& chunk, int startPos) override;

private:
  void SendKeyStateToUI(int note);
  void SendKeyParamsToUI(int note);
  void WriteSettingsFile(const char* filePath) const;
  void ReadSettingsFile(const char* filePath);
  static std::string FolderBasename(const char* path);

  RobinSamplerDSP mDSP;
  std::string     mKeyLabels[128];
  bool            mNeedsUISync = false;
#endif
};
