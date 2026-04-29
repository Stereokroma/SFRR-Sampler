#include "IPlugInstrument.h"
#include "IPlug_include_in_plug_src.h"
#include <string>

// ---------------------------------------------------------------------------
// Black rounded-corner frame drawn as the bottommost layer
class RobinFrameControl : public IControl
{
public:
  RobinFrameControl(const IRECT& b, IColor bg)
  : IControl(b, kNoParameter), mBG(bg) {}

  void Draw(IGraphics& g) override
  {
    g.FillRect(COLOR_BLACK, mRECT);
    g.FillRoundRect(mBG, mRECT.GetPadded(-10.f), 10.f);
    g.DrawRoundRect(IColor(255, 50, 50, 50), mRECT.GetPadded(-10.f), 10.f, nullptr, 1.f);
  }

  // Pass all mouse events through to controls behind it
  bool IsHit(float /*x*/, float /*y*/) const override { return false; }

private:
  IColor mBG;
};

// ---------------------------------------------------------------------------
class RobinClearButton : public IControl
{
public:
  RobinClearButton(const IRECT& b, const IVStyle& style)
  : IControl(b), mStyle(style) {}

  void Draw(IGraphics& g) override
  {
    using namespace igraphics;
    const IColor bg = mMouseIsOver ? IColor(255,75,75,75) : IColor(255,52,52,52);
    g.FillRoundRect(bg, mRECT, 3.f);
    g.DrawRoundRect(IColor(255,70,70,70), mRECT, 3.f, nullptr, 1.f);
    IText t(12.f, IColor(255,210,210,210));
    t.mAlign  = EAlign::Center;
    t.mVAlign = EVAlign::Middle;
    g.DrawText(t, "Clear All", mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mMenu.Clear();
    mMenu.AddItem("Cancel");
    mMenu.AddItem("Yes, clear all keys");
    GetUI()->CreatePopupMenu(*this, mMenu, x, y);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override { SetDirty(false); }
  void OnMouseOut() override { SetDirty(false); }

  void OnPopupMenuSelection(IPopupMenu* pMenu, int) override
  {
    if (pMenu && pMenu->GetChosenItemIdx() == 1)
      GetDelegate()->SendArbitraryMsgFromUI(igraphics::kMsgTagClearAll, kNoTag, 0, nullptr);
    SetDirty(false);
  }

private:
  IVStyle    mStyle;
  IPopupMenu mMenu;
};

// ---------------------------------------------------------------------------

IPlugInstrument::IPlugInstrument(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamMode)->InitEnum("Mode", 0, {"Cycle", "Random"});
  GetParam(kParamVelMode)->InitEnum("Velocity", 0, {"Vel On", "Vel Off"});
  GetParam(kParamSustain)->InitEnum("Sustain", 0, {"Sus Off", "Sus On"});

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("CourierNew",
      "/System/Library/Fonts/Supplemental/Courier New.ttf");

    // Frame is drawn first (background layer)
    pGraphics->AttachControl(
      new RobinFrameControl(pGraphics->GetBounds(), IColor(255, 28, 28, 28)));

    // All layout fits within the 10px frame inset (negative = shrink in iPlug2)
    const IRECT b      = pGraphics->GetBounds().GetPadded(-10.f);
    const IRECT header = b.GetFromTop(64.f);
    const IRECT body   = b.GetReducedFromTop(64.f).GetPadded(-4.f);

    IVStyle tabStyle = DEFAULT_STYLE
      .WithColor(kBG,  IColor(255,  28,  28,  28))
      .WithColor(kFG,  IColor(255,  52,  52,  52))
      .WithColor(kPR,  IColor(255,   0, 175, 130))
      .WithColor(kFR,  IColor(255,  65,  65,  65))
      .WithColor(kHL,  IColor(35,  255, 255, 255))
      .WithDrawShadows(false)
      .WithValueText(IText(13.f, IColor(255, 220, 220, 220)));

    // Header controls
    const IRECT clearR = header.GetFromRight(86.f).GetCentredInside(86.f, 28.f);
    pGraphics->AttachControl(new RobinClearButton(clearR, tabStyle));

    const IRECT midZone = header.GetReducedFromRight(94.f).GetReducedFromLeft(340.f);
    const IRECT totalR  = midZone.GetCentredInside(491.f, 32.f);
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L, totalR.T, totalR.L+165.f, totalR.B),
      kParamMode, {"Cycle","Random"}, "", tabStyle, EVShape::Rectangle));
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L+173.f, totalR.T, totalR.L+323.f, totalR.B),
      kParamVelMode, {"Vel On","Vel Off"}, "", tabStyle, EVShape::Rectangle));
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L+331.f, totalR.T, totalR.R, totalR.B),
      kParamSustain, {"Sus Off","Sus On"}, "", tabStyle, EVShape::Rectangle));

    IText titleText(22.f, IColor(255, 190, 190, 190));
    titleText.mAlign  = EAlign::Near;
    titleText.mVAlign = EVAlign::Middle;
    pGraphics->AttachControl(
      new ITextControl(header.GetFromLeft(340.f).GetPadded(-14.f,0.f,0.f,0.f),
                       "Simple Fucking Round Robin Sampler", titleText));

    // Selected-key panel (between header and keyboard)
    const float panelH = 120.f;
    const IRECT panelR = body.GetFromTop(panelH);
    const IRECT keyArea = body.GetReducedFromTop(panelH);
    const IRECT hintR = keyArea.GetFromBottom(16.f);
    const IRECT keysR = keyArea.GetReducedFromBottom(16.f);

    auto* panel = new RobinPanelControl(panelR);
    pGraphics->AttachControl(panel, kCtrlTagPanel);

    auto* keyboard = new RobinKeyboardControl(keysR);
    keyboard->SetPanel(panel);
    pGraphics->AttachControl(keyboard, kCtrlTagKeyboard);

    IText hintText(10.f, IColor(255, 90, 90, 90));
    hintText.mAlign  = EAlign::Center;
    hintText.mVAlign = EVAlign::Middle;
    pGraphics->AttachControl(new ITextControl(hintR,
      "Click any key to select and play it  |  click + to load samples",
      hintText));

    // Corner resizer on top of everything
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
  };
#endif
}

#if IPLUG_DSP

void IPlugInstrument::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  mDSP.ProcessBlock(outputs, 2, nFrames);
}

void IPlugInstrument::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
}

void IPlugInstrument::ProcessMidiMsg(const IMidiMsg& msg)
{
  mDSP.ProcessMidiMsg(msg);
}

void IPlugInstrument::OnMidiMsgUI(const IMidiMsg& msg)
{
  SendMidiMsg(msg);
}

void IPlugInstrument::OnParamChange(int paramIdx)
{
  switch (paramIdx) {
    case kParamMode:    mDSP.SetRandomMode(GetParam(kParamMode)->Int() == 1);           break;
    case kParamVelMode: mDSP.SetVelocitySensitive(GetParam(kParamVelMode)->Int() == 0); break;
    case kParamSustain: mDSP.SetSustainMode(GetParam(kParamSustain)->Int() == 1);       break;
    default: break;
  }
}

void IPlugInstrument::OnUIOpen()
{
  for (int n = 0; n < 128; n++) {
    if (mDSP.HasSamples(n)) SendKeyStateToUI(n);
    SendKeyParamsToUI(n);
  }
}

void IPlugInstrument::OnIdle()
{
  if (mNeedsUISync) {
    mNeedsUISync = false;
    for (int n = 0; n < 128; n++) {
      if (mDSP.HasSamples(n)) SendKeyStateToUI(n);
      SendKeyParamsToUI(n);
    }
  }
}

bool IPlugInstrument::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  using namespace igraphics;

  if (msgTag == kMsgTagLoadSamples && dataSize == sizeof(RobinLoadMsg)) {
    const auto* m = static_cast<const RobinLoadMsg*>(pData);
    if (m->note >= 0 && m->note <= 127) {
      mDSP.LoadSamplesForKey(m->note, m->path);
      mKeyLabels[m->note] = FolderBasename(m->path);
      SendKeyStateToUI(m->note);
    }
    return true;
  }

  if (msgTag == kMsgTagSetKeyParam && dataSize == sizeof(RobinKeyParamMsg)) {
    const auto* m = static_cast<const RobinKeyParamMsg*>(pData);
    if (m->note >= 0 && m->note <= 127)
      mDSP.SetKeyParams(m->note, m->gain, m->pitch, m->lead, m->pan);
    return true;
  }

  if (msgTag == kMsgTagClearAll) {
    std::vector<int> loaded;
    for (int n = 0; n < 128; n++)
      if (mDSP.HasSamples(n)) loaded.push_back(n);
    mDSP.ClearAllKeys();
    for (int n = 0; n < 128; n++) mKeyLabels[n].clear();
    for (int n : loaded) SendKeyStateToUI(n);
    // Also sync params (now reset to defaults)
    for (int n : loaded) SendKeyParamsToUI(n);
    return true;
  }

  return false;
}

void IPlugInstrument::SendKeyStateToUI(int note)
{
  using namespace igraphics;
  RobinStateMsg msg;
  msg.note        = note;
  msg.hasSamples  = mDSP.HasSamples(note);
  msg.sampleCount = mDSP.GetSampleCount(note);
  strncpy(msg.label, mKeyLabels[note].c_str(), sizeof(msg.label)-1);
  msg.label[sizeof(msg.label)-1] = '\0';
  SendControlMsgFromDelegate(kCtrlTagKeyboard, kMsgTagSampleLoaded, sizeof(msg), &msg);
  SendControlMsgFromDelegate(kCtrlTagPanel,    kMsgTagSampleLoaded, sizeof(msg), &msg);
}

void IPlugInstrument::SendKeyParamsToUI(int note)
{
  using namespace igraphics;
  RobinKeyParamMsg msg;
  msg.note = note;
  mDSP.GetKeyParams(note, msg.gain, msg.pitch, msg.lead, msg.pan);
  SendControlMsgFromDelegate(kCtrlTagPanel, kMsgTagKeyParamSync, sizeof(msg), &msg);
}

std::string IPlugInstrument::FolderBasename(const char* path)
{
  std::string p = path;
  while (!p.empty() && (p.back()=='/'||p.back()=='\\')) p.pop_back();
  size_t pos = p.rfind('/');
  if (pos == std::string::npos) pos = p.rfind('\\');
  return (pos == std::string::npos) ? p : p.substr(pos+1);
}

bool IPlugInstrument::SerializeState(IByteChunk& chunk) const
{
  SerializeParams(chunk);
  mDSP.SerializeKeyStates(chunk);
  for (int n = 0; n < 128; n++)
    chunk.PutStr(mKeyLabels[n].c_str());
  return true;
}

int IPlugInstrument::UnserializeState(const IByteChunk& chunk, int startPos)
{
  startPos = UnserializeParams(chunk, startPos);
  if (startPos < 0) return startPos;
  startPos = mDSP.UnserializeKeyStates(chunk, startPos);
  if (startPos < 0) return startPos;
  for (int n = 0; n < 128; n++) {
    WDL_String label;
    int newPos = chunk.GetStr(label, startPos);
    if (newPos < 0) break;
    startPos = newPos;
    mKeyLabels[n] = label.Get();
  }
  mNeedsUISync = true;
  return startPos;
}

#endif
