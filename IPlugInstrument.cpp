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
    IText t(12.f, IColor(255, 255, 249, 235), "Roboto-Regular");
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
class RobinPresetButton : public IControl
{
public:
  RobinPresetButton(const IRECT& b, bool isStore)
  : IControl(b, kNoParameter), mIsStore(isStore) {}

  void Draw(IGraphics& g) override
  {
    const IColor bg = mMouseIsOver ? IColor(255,75,75,75) : IColor(255,52,52,52);
    g.FillRoundRect(bg, mRECT, 3.f);
    g.DrawRoundRect(IColor(255,70,70,70), mRECT, 3.f, nullptr, 1.f);
    IText t(12.f, IColor(255, 255, 249, 235), "Roboto-Regular");
    t.mAlign  = EAlign::Center;
    t.mVAlign = EVAlign::Middle;
    g.DrawText(t, mIsStore ? "Store Settings" : "Load Settings", mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    using namespace igraphics;
    WDL_String fn, dir;
    EFileAction action = mIsStore ? EFileAction::Save : EFileAction::Open;
    int tag = mIsStore ? kMsgTagStoreSettings : kMsgTagLoadSettings;
    GetUI()->PromptForFile(fn, dir, action, "sfrr",
      [this, tag](const WDL_String& fileName, const WDL_String&) {
        if (fileName.GetLength() == 0) return;
        GetDelegate()->SendArbitraryMsgFromUI(tag, kNoTag,
          fileName.GetLength() + 1, fileName.Get());
      });
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override { SetDirty(false); }
  void OnMouseOut() override { SetDirty(false); }

private:
  bool mIsStore;
};

// ---------------------------------------------------------------------------

IPlugInstrument::IPlugInstrument(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamMode)->InitEnum("Mode", 0, {"Cycle", "Random"});
  GetParam(kParamVelMode)->InitEnum("Velocity", 0, {"Vel On", "Vel Off"});
  GetParam(kParamSustain)->InitEnum("Sustain", 0, {"Sus On", "Sus Off"});

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->EnableMouseOver(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("CourierNew",
#ifdef OS_WIN
      "C:\\Windows\\Fonts\\cour.ttf"
#else
      "/System/Library/Fonts/Supplemental/Courier New.ttf"
#endif
    );
    pGraphics->LoadFont("CourierNew-Bold",
#ifdef OS_WIN
      "C:\\Windows\\Fonts\\courbd.ttf"
#else
      "/System/Library/Fonts/Supplemental/Courier New Bold.ttf"
#endif
    );

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
      .WithColor(kPR,  IColor(255, 122, 183,  97))
      .WithColor(kFR,  IColor(255,  65,  65,  65))
      .WithColor(kHL,  IColor(35,  255, 255, 255))
      .WithDrawShadows(false)
      .WithValueText(IText(13.f, IColor(255, 255, 249, 235)));

    // Header controls — Clear All with 10px spacer from right edge
    const IRECT clearZone = header.GetFromRight(96.f).GetReducedFromRight(10.f);
    const IRECT clearR = clearZone.GetCentredInside(86.f, 28.f);
    pGraphics->AttachControl(new RobinClearButton(clearR, tabStyle));

    const IRECT midZone = header.GetReducedFromRight(94.f).GetReducedFromLeft(355.f);
    const IRECT totalR  = midZone.GetCentredInside(466.f, 32.f);
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L, totalR.T, totalR.L+140.f, totalR.B),
      kParamMode, {"Cycle","Random"}, "", tabStyle, EVShape::Rectangle));
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L+148.f, totalR.T, totalR.L+298.f, totalR.B),
      kParamVelMode, {"Vel On","Vel Off"}, "", tabStyle, EVShape::Rectangle));
    pGraphics->AttachControl(new IVTabSwitchControl(
      IRECT(totalR.L+306.f, totalR.T, totalR.R, totalR.B),
      kParamSustain, {"Sus On","Sus Off"}, "", tabStyle, EVShape::Rectangle));

    // Stereokroma logo — SVG scales crisp at any resolution
    struct LogoControl : public IControl {
      ISVG mSVG;
      LogoControl(const IRECT& b, const ISVG& svg) : IControl(b, kNoParameter), mSVG(svg) {}
      void Draw(IGraphics& g) override { g.DrawSVG(mSVG, mRECT); }
      bool IsHit(float, float) const override { return false; }
    };
    ISVG logoSVG = pGraphics->LoadSVG(STEREOKROMA_LOGO_FN);
    pGraphics->AttachControl(new LogoControl(IRECT(b.L + 12.f, b.T + 12.f, b.L + 52.f, b.T + 52.f), logoSVG));

    IText titleText(22.f, IColor(255, 255, 249, 235));
    titleText.mAlign  = EAlign::Near;
    titleText.mVAlign = EVAlign::Middle;
    pGraphics->AttachControl(
      new ITextControl(header.GetReducedFromLeft(58.f).GetFromLeft(297.f).GetPadded(-4.f,0.f,0.f,0.f),
                       "Simple Fucking Round Robin Sampler", titleText));

    // Selected-key panel (between header and keyboard)
    const float panelH  = 120.f;
    const float presetH = 44.f;
    const IRECT panelR  = body.GetFromTop(panelH);
    const IRECT presetR = body.GetFromBottom(presetH);
    const IRECT keyArea = body.GetReducedFromTop(panelH).GetReducedFromBottom(presetH);

    auto* panel = new RobinPanelControl(panelR);
    pGraphics->AttachControl(panel, kCtrlTagPanel);

    auto* keyboard = new RobinKeyboardControl(keyArea);
    keyboard->SetPanel(panel);
    pGraphics->AttachControl(keyboard, kCtrlTagKeyboard);

    // Preset bar — Store / Load buttons bottom-right, matching Clear All's right margin
    {
      const float btnW = 130.f, btnH = 24.f, btnGap = 8.f;
      const float btnY = presetR.MH() - btnH * 0.5f;
      const float rEdge = presetR.R - 10.f;
      pGraphics->AttachControl(new RobinPresetButton(
        IRECT(rEdge - btnW, btnY, rEdge, btnY + btnH), false));
      pGraphics->AttachControl(new RobinPresetButton(
        IRECT(rEdge - btnW*2.f - btnGap, btnY, rEdge - btnW - btnGap, btnY + btnH), true));
    }

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
    case kParamSustain: mDSP.SetSustainMode(GetParam(kParamSustain)->Int() == 0);       break;
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
      mDSP.SetKeyParams(m->note, m->gain, m->pitch, m->lead, m->pan, m->attack, m->release);
    return true;
  }

  if (msgTag == kMsgTagStoreSettings && dataSize > 0) {
    WriteSettingsFile(static_cast<const char*>(pData));
    return true;
  }

  if (msgTag == kMsgTagLoadSettings && dataSize > 0) {
    ReadSettingsFile(static_cast<const char*>(pData));
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
  mDSP.GetKeyParams(note, msg.gain, msg.pitch, msg.lead, msg.pan, msg.attack, msg.release);
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

void IPlugInstrument::WriteSettingsFile(const char* filePath) const
{
  FILE* f = fopen(filePath, "w");
  if (!f) return;
  fprintf(f, "SFRR_PRESET_V1\n");
  fprintf(f, "mode=%d\n",    GetParam(kParamMode)->Int());
  fprintf(f, "velmode=%d\n", GetParam(kParamVelMode)->Int());
  fprintf(f, "sustain=%d\n", GetParam(kParamSustain)->Int());
  for (int n = 0; n < 128; n++) {
    if (!mDSP.HasSamples(n)) continue;
    float gain=1.f, pitch=0.f, lead=0.f, pan=0.f, attack=0.f, release=500.f;
    mDSP.GetKeyParams(n, gain, pitch, lead, pan, attack, release);
    fprintf(f, "note_%d.path=%s\n",    n, mDSP.GetFolderPath(n).c_str());
    fprintf(f, "note_%d.gain=%f\n",    n, gain);
    fprintf(f, "note_%d.pitch=%f\n",   n, pitch);
    fprintf(f, "note_%d.lead=%f\n",    n, lead);
    fprintf(f, "note_%d.pan=%f\n",     n, pan);
    fprintf(f, "note_%d.attack=%f\n",  n, attack);
    fprintf(f, "note_%d.release=%f\n", n, release);
  }
  fclose(f);
}

void IPlugInstrument::ReadSettingsFile(const char* filePath)
{
  FILE* f = fopen(filePath, "r");
  if (!f) return;

  char line[2048];
  auto stripNewline = [](char* s) {
    for (char* p = s + strlen(s) - 1; p >= s && (*p=='\n'||*p=='\r'); --p) *p = '\0';
  };

  if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
  stripNewline(line);
  if (strcmp(line, "SFRR_PRESET_V1") != 0) { fclose(f); return; }

  // Track previously loaded notes so we can clear them in the UI
  bool hadSamples[128] = {};
  for (int n = 0; n < 128; n++) hadSamples[n] = mDSP.HasSamples(n);

  mDSP.ClearAllKeys();
  for (int n = 0; n < 128; n++) mKeyLabels[n].clear();

  // Accumulate per-note params (applied after all paths are loaded)
  float gains[128], pitches[128], leads[128], pans[128], attacks[128], releases[128];
  bool  hasParam[128] = {};
  for (int n = 0; n < 128; n++) {
    gains[n]=1.f; pitches[n]=0.f; leads[n]=0.f; pans[n]=0.f;
    attacks[n]=0.f; releases[n]=500.f;
  }

  int modeVal = GetParam(kParamMode)->Int();
  int velVal  = GetParam(kParamVelMode)->Int();
  int susVal  = GetParam(kParamSustain)->Int();

  while (fgets(line, sizeof(line), f)) {
    stripNewline(line);
    if      (strncmp(line, "mode=",    5)==0) modeVal = atoi(line+5);
    else if (strncmp(line, "velmode=", 8)==0) velVal  = atoi(line+8);
    else if (strncmp(line, "sustain=", 8)==0) susVal  = atoi(line+8);
    else if (strncmp(line, "note_",    5)==0) {
      int   note = atoi(line+5);
      if (note < 0 || note > 127) continue;
      char* dot  = strchr(line+5, '.');
      char* eq   = dot ? strchr(dot+1, '=') : nullptr;
      if (!dot || !eq) continue;
      size_t     klen = (size_t)(eq - dot - 1);
      const char* key = dot+1;
      const char* val = eq+1;
      if      (klen==4 && !strncmp(key,"path",    4)) {
        mDSP.LoadSamplesForKey(note, val);
        mKeyLabels[note] = FolderBasename(val);
      } else if (klen==4 && !strncmp(key,"gain",   4)) { gains[note]    = (float)atof(val); hasParam[note]=true; }
      else if   (klen==5 && !strncmp(key,"pitch",  5)) { pitches[note]  = (float)atof(val); hasParam[note]=true; }
      else if   (klen==4 && !strncmp(key,"lead",   4)) { leads[note]    = (float)atof(val); hasParam[note]=true; }
      else if   (klen==3 && !strncmp(key,"pan",    3)) { pans[note]     = (float)atof(val); hasParam[note]=true; }
      else if   (klen==6 && !strncmp(key,"attack", 6)) { attacks[note]  = (float)atof(val); hasParam[note]=true; }
      else if   (klen==7 && !strncmp(key,"release",7)) { releases[note] = (float)atof(val); hasParam[note]=true; }
    }
  }
  fclose(f);

  // Apply per-note params
  for (int n = 0; n < 128; n++)
    if (hasParam[n]) mDSP.SetKeyParams(n, gains[n], pitches[n], leads[n], pans[n], attacks[n], releases[n]);

  // Apply global params and push to UI controls
  GetParam(kParamMode)->Set(modeVal);    OnParamChange(kParamMode);
  GetParam(kParamVelMode)->Set(velVal);  OnParamChange(kParamVelMode);
  GetParam(kParamSustain)->Set(susVal);  OnParamChange(kParamSustain);
  SendCurrentParamValuesFromDelegate();

  // Sync key state to UI
  for (int n = 0; n < 128; n++)
    if (hadSamples[n] || mDSP.HasSamples(n)) {
      SendKeyStateToUI(n);
      SendKeyParamsToUI(n);
    }
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
