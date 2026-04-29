#pragma once

#include "RobinPanelControl.h"   // shared types + panel class
#include "IPlugMidi.h"
#include <vector>
#include <cstring>
#include <algorithm>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class RobinKeyboardControl : public IControl
{
public:
  RobinKeyboardControl(const IRECT& bounds, int minNote = 36, int maxNote = 96)
  : IControl(bounds, kNoParameter)
  , mMinNote(minNote)
  , mMaxNote(maxNote)
  {
    memset(mHasSamples,   0, sizeof(mHasSamples));
    memset(mPressedKeys,  0, sizeof(mPressedKeys));
    memset(mKeyLabels,    0, sizeof(mKeyLabels));
    memset(mSampleCounts, 0, sizeof(mSampleCounts));
    SetWantsMidi(true);
    ComputeLayout();
  }

  void SetPanel(RobinPanelControl* p) { mPanel = p; }

  void OnResize() override { ComputeLayout(); SetDirty(false); }

  void Draw(IGraphics& g) override
  {
    g.FillRect(COLOR_BLACK, mRECT);
    const float bkH = mRECT.H() * kBKHeightRatio;

    // White keys
    for (int i = 0; i < NKeys(); i++) {
      if (IsBlackNote(mMinNote + i)) continue;
      const int   note = mMinNote + i;
      const float kL   = mKeyXPos[i];
      const IRECT r(kL, mRECT.T, kL + mWKWidth, mRECT.B);

      // Fill: selected → light blue tint, pressed → press color, else white
      const bool isSelected = (note == mSelectedKey);
      const bool isPressed  = mPressedKeys[i];
      IColor fill = isPressed ? kPressedWK
                  : isSelected ? kSelectedWK
                  : kWhiteKey;
      g.FillRect(fill, r);
      g.DrawRect(kFrame, r, nullptr, 1.f);

      if (note < 128 && mHasSamples[note]) {
        IColor bar = kLoadedColor; bar.A = 220;
        g.FillRect(bar, r.GetFromBottom(kLoadedBarH));
        if (mKeyLabels[note][0] != '\0') {
          IText lbl(10.f, IColor(255, 25, 25, 25), kRobinMonoFont);
          lbl.mAngle  = -90.f;
          lbl.mAlign  = EAlign::Center;
          lbl.mVAlign = EVAlign::Bottom;
          g.DrawText(lbl, mKeyLabels[note], GetLabelRect(i));
        }
      }

      DrawPlusButton(g, GetPlusButtonRect(i), i == mHoverPlusKey,
                     false, note < 128 ? mSampleCounts[note] : 0);
    }

    // Black keys
    for (int i = 0; i < NKeys(); i++) {
      if (!IsBlackNote(mMinNote + i)) continue;
      const int   note = mMinNote + i;
      const float kL   = mKeyXPos[i];
      const IRECT r(kL, mRECT.T, kL + mBKWidth, mRECT.T + bkH);

      const bool isSelected = (note == mSelectedKey);
      const bool isPressed  = mPressedKeys[i];
      IColor fill = isPressed ? kPressedBK
                  : isSelected ? kSelectedBK
                  : kBlackKey;
      g.FillRect(fill, r);
      g.DrawRect(kFrame, r, nullptr, 0.5f);

      if (note < 128 && mHasSamples[note]) {
        IColor bar = kLoadedColor; bar.A = 230;
        g.FillRect(bar, r.GetFromBottom(kLoadedBarH));
        if (mKeyLabels[note][0] != '\0') {
          IText lbl(8.5f, IColor(255, 255, 249, 235), kRobinMonoFont);
          lbl.mAngle  = -90.f;
          lbl.mAlign  = EAlign::Center;
          lbl.mVAlign = EVAlign::Bottom;
          g.DrawText(lbl, mKeyLabels[note], GetLabelRect(i));
        }
      }

      DrawPlusButton(g, GetPlusButtonRect(i), i == mHoverPlusKey,
                     true, note < 128 ? mSampleCounts[note] : 0);
    }
  }

  // Mouse: click selects key AND plays note
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mod.R) return;

    // "+" button — open folder picker (does NOT trigger note)
    for (int i = 0; i < NKeys(); i++)
      if (IsBlackNote(mMinNote+i) && GetPlusButtonRect(i).Contains(x,y))
        { OpenFolderPicker(mMinNote+i); return; }
    for (int i = 0; i < NKeys(); i++)
      if (!IsBlackNote(mMinNote+i) && GetPlusButtonRect(i).Contains(x,y))
        { OpenFolderPicker(mMinNote+i); return; }

    // Key area: select + play
    int key = GetKeyAtPoint(x, y);
    if (key < 0) return;

    // Select this key and inform the panel
    const int note = mMinNote + key;
    if (note != mSelectedKey) {
      mSelectedKey = note;
      if (mPanel) mPanel->SelectKey(note);
    }

    // Play
    mLastTouchedKey   = key;
    mPressedKeys[key] = true;
    float vel = Clip((y - mRECT.T) / (mRECT.H() * 0.9f), 1.f/127.f, 1.f);
    SendMidi(note, (int)(vel * 127.f));
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    if (mLastTouchedKey >= 0) {
      mPressedKeys[mLastTouchedKey] = false;
      SendMidi(mMinNote + mLastTouchedKey, 0);
      mLastTouchedKey = -1;
      SetDirty(false);
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    int key = GetKeyAtPoint(x, y);
    if (key == mLastTouchedKey) return;
    if (mLastTouchedKey >= 0) {
      mPressedKeys[mLastTouchedKey] = false;
      SendMidi(mMinNote + mLastTouchedKey, 0);
    }
    mLastTouchedKey = key;
    if (key >= 0) {
      mPressedKeys[key] = true;
      float vel = Clip((y - mRECT.T) / (mRECT.H() * 0.9f), 1.f/127.f, 1.f);
      SendMidi(mMinNote + key, (int)(vel * 127.f));
    }
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    int ph = -1;
    for (int i = 0; i < NKeys(); i++)
      if (GetPlusButtonRect(i).Contains(x,y)) { ph = i; break; }
    if (ph != mHoverPlusKey) { mHoverPlusKey = ph; SetDirty(false); }
  }

  void OnMouseOut() override
  {
    if (mLastTouchedKey >= 0) {
      mPressedKeys[mLastTouchedKey] = false;
      SendMidi(mMinNote + mLastTouchedKey, 0);
      mLastTouchedKey = -1;
    }
    mHoverPlusKey = -1;
    SetDirty(false);
  }

  void OnMidi(const IMidiMsg& msg) override
  {
    int key = msg.NoteNumber() - mMinNote;
    if (key < 0 || key >= NKeys()) return;
    switch (msg.StatusMsg()) {
      case IMidiMsg::kNoteOn:  mPressedKeys[key] = (msg.Velocity() > 0); break;
      case IMidiMsg::kNoteOff: mPressedKeys[key] = false; break;
      default: break;
    }
    SetDirty(false);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (msgTag == kMsgTagSampleLoaded && dataSize == sizeof(RobinStateMsg)) {
      const auto* m = static_cast<const RobinStateMsg*>(pData);
      if (m->note >= 0 && m->note < 128) {
        mHasSamples[m->note]   = m->hasSamples;
        mSampleCounts[m->note] = m->sampleCount;
        strncpy(mKeyLabels[m->note], m->label, 63);
        mKeyLabels[m->note][63] = '\0';
        SetDirty(false);
      }
    }
  }

private:
  static constexpr float kBKWidthRatio  = 0.6f;
  static constexpr float kBKHeightRatio = 0.62f;
  static constexpr float kLoadedBarH    = 5.f;

  static const IColor kWhiteKey, kBlackKey;
  static const IColor kPressedWK, kPressedBK;
  static const IColor kSelectedWK, kSelectedBK;
  static const IColor kFrame, kLoadedColor;

  int   NKeys()         const { return mMaxNote - mMinNote + 1; }
  bool  IsBlackNote(int n) const {
    int pc = n % 12; return pc==1||pc==3||pc==6||pc==8||pc==10;
  }
  float BlackKeyShift(int n) const {
    switch (n % 12) {
      case 1:  return 7.f/12.f; case 3:  return 5.f/12.f;
      case 6:  return 2.f/3.f;  case 8:  return 0.5f;
      case 10: return 1.f/3.f;  default: return 0.f;
    }
  }

  void ComputeLayout()
  {
    int n = NKeys();
    mKeyXPos.resize(n);
    int numW = 0;
    for (int i = 0; i < n; i++) if (!IsBlackNote(mMinNote+i)) numW++;
    if (numW == 0) return;
    mWKWidth = mRECT.W() / (float)numW;
    mBKWidth = mWKWidth * kBKWidthRatio;
    float wkL = mRECT.L;
    for (int i = 0; i < n; i++) {
      int note = mMinNote + i;
      if (!IsBlackNote(note)) { mKeyXPos[i] = wkL; wkL += mWKWidth; }
      else                     mKeyXPos[i] = wkL - BlackKeyShift(note) * mBKWidth;
    }
  }

  int GetKeyAtPoint(float x, float y) const
  {
    const float bkH = mRECT.H() * kBKHeightRatio;
    for (int i = 0; i < NKeys(); i++) {
      if (!IsBlackNote(mMinNote+i)) continue;
      float kL = mKeyXPos[i];
      if (x>=kL && x<kL+mBKWidth && y>=mRECT.T && y<mRECT.T+bkH) return i;
    }
    for (int i = 0; i < NKeys(); i++) {
      if (IsBlackNote(mMinNote+i)) continue;
      float kL = mKeyXPos[i];
      if (x>=kL && x<kL+mWKWidth && y>=mRECT.T && y<mRECT.B) return i;
    }
    return -1;
  }

  IRECT GetPlusButtonRect(int i) const
  {
    const int   note = mMinNote + i;
    const float kL   = mKeyXPos[i];
    if (IsBlackNote(note)) {
      float sz = std::max(8.f, std::min(11.f, mBKWidth - 2.f));
      float cx = kL + mBKWidth * 0.5f;
      float cy = mRECT.T + sz * 0.5f + 3.f;
      return IRECT(cx-sz*.5f, cy-sz*.5f, cx+sz*.5f, cy+sz*.5f);
    } else {
      float sz = std::max(10.f, std::min(14.f, mWKWidth - 4.f));
      float cx = kL + mWKWidth * 0.5f;
      float cy = mRECT.B - kLoadedBarH - 2.f - sz * 0.5f;
      return IRECT(cx-sz*.5f, cy-sz*.5f, cx+sz*.5f, cy+sz*.5f);
    }
  }

  IRECT GetLabelRect(int i) const
  {
    const int   note  = mMinNote + i;
    const float kL    = mKeyXPos[i];
    const IRECT plusR = GetPlusButtonRect(i);
    if (IsBlackNote(note)) {
      const float bot = mRECT.T + mRECT.H() * kBKHeightRatio - kLoadedBarH - 4.f;
      const float top = plusR.B + 3.f;
      if (bot <= top) return IRECT();
      return IRECT(kL, top, kL + mBKWidth, bot);
    } else {
      const float bot      = plusR.T - 5.f;
      const float availTop = mRECT.T + mRECT.H() * kBKHeightRatio + 2.f;
      if (bot <= availTop) return IRECT();
      return IRECT(kL+1.f, availTop, kL+mWKWidth-1.f, bot);
    }
  }

  void DrawPlusButton(IGraphics& g, const IRECT& r, bool hovered, bool onBlack, int count)
  {
    if (r.Empty()) return;
    const bool loaded = count > 0;
    IColor bg;
    if (loaded)
      bg = onBlack ? IColor(hovered?210:160, 97,159,75) : IColor(hovered?230:180, 122,183,97);
    else
      bg = onBlack ? IColor(hovered?200:140, 120,120,120) : IColor(hovered?220:160, 90,90,90);
    g.FillRoundRect(bg, r, 2.f);
    const IColor fg = loaded ? IColor(255,255,255,255)
      : (onBlack ? IColor(255, hovered?235:185, hovered?235:185, hovered?235:185)
                 : IColor(255, hovered?255:215, hovered?255:215, hovered?255:215));
    char buf[8];
    if (loaded) snprintf(buf, sizeof(buf), "%d", count);
    else        snprintf(buf, sizeof(buf), "+");
    IText t(r.H() * (loaded ? 0.82f : 0.9f), fg, kRobinMonoFont);
    t.mAlign  = EAlign::Center;
    t.mVAlign = EVAlign::Middle;
    g.DrawText(t, buf, r);
  }

  void OpenFolderPicker(int note)
  {
    WDL_String dir;
    GetUI()->PromptForDirectory(dir,
      [this, note](const WDL_String&, const WDL_String& path) {
        if (path.GetLength() == 0) return;
        RobinLoadMsg msg;
        msg.note = note;
        strncpy(msg.path, path.Get(), sizeof(msg.path)-1);
        msg.path[sizeof(msg.path)-1] = '\0';
        GetDelegate()->SendArbitraryMsgFromUI(kMsgTagLoadSamples, GetTag(), sizeof(msg), &msg);
        SetDirty(false);
      });
  }

  void SendMidi(int note, int velocity)
  {
    IMidiMsg msg;
    if (velocity > 0) msg.MakeNoteOnMsg(note, velocity, 0);
    else              msg.MakeNoteOffMsg(note, 0);
    GetDelegate()->SendMidiMsgFromUI(msg);
  }

  int   mMinNote, mMaxNote;
  std::vector<float> mKeyXPos;
  float mWKWidth = 0.f, mBKWidth = 0.f;

  bool  mHasSamples[128]    = {};
  bool  mPressedKeys[128]   = {};
  char  mKeyLabels[128][64] = {};
  int   mSampleCounts[128]  = {};

  int   mSelectedKey  = -1;
  int   mLastTouchedKey = -1;
  int   mHoverPlusKey   = -1;

  RobinPanelControl* mPanel = nullptr;
};

const IColor RobinKeyboardControl::kWhiteKey   = IColor(255, 238, 238, 238);
const IColor RobinKeyboardControl::kBlackKey   = IColor(255,  35,  35,  35);
const IColor RobinKeyboardControl::kPressedWK  = IColor(255, 130, 185, 255);
const IColor RobinKeyboardControl::kPressedBK  = IColor(255,  55, 120, 205);
const IColor RobinKeyboardControl::kSelectedWK = IColor(255, 195, 220, 255);  // light blue-white
const IColor RobinKeyboardControl::kSelectedBK = IColor(255,  45,  65,  95);  // dark blue-black
const IColor RobinKeyboardControl::kFrame      = IColor(255,  80,  80,  80);
const IColor RobinKeyboardControl::kLoadedColor= IColor(255, 122, 183,  97);

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
