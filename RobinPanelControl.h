#pragma once

#include "IControl.h"
#include <cstring>
#include <cstdio>
#include <cmath>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

// ---------------------------------------------------------------------------
// Shared message tags and structs (used by both panel and keyboard controls)
// ---------------------------------------------------------------------------

enum ERobinMsgTags
{
  kMsgTagLoadSamples = 0,
  kMsgTagSampleLoaded,
  kMsgTagClearAll,
  kMsgTagSetKeyParam,
  kMsgTagKeyParamSync,
  kMsgTagStoreSettings,
  kMsgTagLoadSettings,
};

struct RobinLoadMsg   { int note; char path[1024]; };
struct RobinStateMsg  { int note; bool hasSamples; char label[64]; int sampleCount; };
struct RobinKeyParamMsg { int note; float gain; float pitch; float lead; float pan; };

static constexpr const char* kRobinMonoFont     = "CourierNew";
static constexpr const char* kRobinMonoBoldFont = "CourierNew-Bold";
static constexpr float kRobinPI = 3.14159265f;

// ---------------------------------------------------------------------------
// RobinPanelControl — selected-key editor with 4 rotary knobs
// ---------------------------------------------------------------------------

class RobinPanelControl : public IControl
{
public:
  static constexpr int   kNumKnobs   = 4;
  static constexpr float kKnobR      = 30.f;   // outer radius of arc ring
  static constexpr float kBodyR      = 22.f;   // filled knob body radius
  static constexpr float kArcR       = 26.f;   // arc ring center radius
  static constexpr float kArcThick   = 5.f;
  static constexpr float kAngleStart = 120.f;  // degrees, 7 o'clock (CW from east)
  static constexpr float kAngleSweep = 300.f;
  static constexpr float kAngleCenter = kAngleStart + kAngleSweep * 0.5f; // 270° = 12 o'clock
  static constexpr float kInfoW      = 210.f;  // width of left info section
  static constexpr float kDragScale  = 220.f;  // pixels for full-range drag

  struct KnobDef {
    const char* label;
    float minVal, maxVal, defVal;
    float dispMult;  // stored × dispMult = display number
    const char* unit;
    bool  bipolar;
    int   valIdx;    // index into mVals[n]: 0=gain 1=pitch 2=lead 3=pan
  };

  // Display order: Trim, Gain, Pitch, Pan
  static constexpr KnobDef kKnobs[kNumKnobs] = {
    { "Trim",   0.f,  500.f, 0.f,   1.f, "ms",  false, 2 },  // lead
    { "Gain",   0.f,   2.f,  1.f, 100.f, "%",   false, 0 },
    { "Pitch", -12.f, 12.f,  0.f,   1.f, " st", true,  1 },
    { "Pan",   -1.f,  1.f,   0.f, 100.f, "",    true,  3 },
  };

  static const IColor kKnobArcColors[kNumKnobs];

  RobinPanelControl(const IRECT& bounds)
  : IControl(bounds, kNoParameter)
  {
    mDblAsSingleClick = false;
    for (int n = 0; n < 128; n++) {
      for (int k = 0; k < kNumKnobs; k++) mVals[n][kKnobs[k].valIdx] = kKnobs[k].defVal;
      memset(mLabels[n], 0, 64);
    }
    memset(mHasSamples, 0, sizeof(mHasSamples));
    memset(mCounts,     0, sizeof(mCounts));
  }

  // Called by the keyboard control when a key is clicked
  void SelectKey(int note)
  {
    mSelectedKey = note;
    SetDirty(false);
  }

  // -------------------------------------------------------------------------
  void Draw(IGraphics& g) override
  {
    const bool has = (mSelectedKey >= 0 && mSelectedKey < 128);

    // Background
    g.FillRect(IColor(255, 30, 30, 30), mRECT);
    g.DrawLine(IColor(255, 55, 55, 55), mRECT.L, mRECT.B - 1.f, mRECT.R, mRECT.B - 1.f);

    // Separator between info and knobs
    const float sepX = mRECT.L + kInfoW;
    g.DrawLine(IColor(255, 50, 50, 50), sepX, mRECT.T + 8.f, sepX, mRECT.B - 8.f);

    // ---- Info section ------------------------------------------------------
    const IRECT infoR(mRECT.L, mRECT.T, sepX, mRECT.B);

    if (!has) {
      IText t(12.f, IColor(255, 207, 204, 207), kRobinMonoFont);
      t.mAlign  = EAlign::Center;
      t.mVAlign = EVAlign::Middle;
      g.DrawText(t, "Click a key to edit", infoR);
    } else {
      static const char* kNoteNames[] =
        {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      char noteName[8];
      snprintf(noteName, sizeof(noteName), "%s%d",
               kNoteNames[mSelectedKey % 12], mSelectedKey / 12 - 1);

      // Large note name — bold and larger
      IText bigT(34.f, IColor(255, 255, 249, 235), kRobinMonoBoldFont);
      bigT.mAlign  = EAlign::Near;
      bigT.mVAlign = EVAlign::Middle;
      g.DrawText(bigT, noteName, IRECT(infoR.L+12.f, infoR.T, infoR.L+100.f, infoR.MH()));

      // Sample count badge
      char cntBuf[24];
      if (mCounts[mSelectedKey] > 0)
        snprintf(cntBuf, sizeof(cntBuf), "%d Samples", mCounts[mSelectedKey]);
      else
        snprintf(cntBuf, sizeof(cntBuf), "No Samples");
      IColor cntCol = mCounts[mSelectedKey] > 0
        ? IColor(255, 122, 183, 97) : IColor(255, 202, 25, 9);
      IText cntT(14.f, cntCol, kRobinMonoFont);
      cntT.mAlign  = EAlign::Near;
      cntT.mVAlign = EVAlign::Middle;
      g.DrawText(cntT, cntBuf, IRECT(infoR.L+12.f, infoR.MH() - 4.f, infoR.R-4.f, infoR.MH() + 20.f));

      // Folder label
      if (mLabels[mSelectedKey][0] != '\0') {
        IText lblT(11.f, IColor(255, 207, 204, 207), kRobinMonoFont);
        lblT.mAlign  = EAlign::Near;
        lblT.mVAlign = EVAlign::Middle;
        g.DrawText(lblT, mLabels[mSelectedKey],
                   IRECT(infoR.L+12.f, infoR.MH() + 18.f, infoR.R-6.f, infoR.B));
      }
    }

    // ---- Knobs -------------------------------------------------------------
    const int dimA = has ? 255 : 60;  // alpha multiplier when no key

    for (int k = 0; k < kNumKnobs; k++) {
      float cx, cy;
      GetKnobCenter(k, cx, cy);

      const float storedVal = has ? mVals[mSelectedKey][kKnobs[k].valIdx] : kKnobs[k].defVal;
      const float t = (storedVal - kKnobs[k].minVal)
                    / (kKnobs[k].maxVal - kKnobs[k].minVal);

      const bool active = has && (mHoveredKnob == k || mDragKnob == k);

      DrawKnob(g, k, cx, cy, t, active, dimA);

      // Label
      IRECT lblR = GetKnobLabelRect(k);
      IText lt(11.f, IColor(dimA, 207, 204, 207), kRobinMonoFont);
      lt.mAlign  = EAlign::Center;
      lt.mVAlign = EVAlign::Middle;
      g.DrawText(lt, kKnobs[k].label, lblR);

      // Value
      char valBuf[16];
      FormatVal(k, storedVal, valBuf, sizeof(valBuf));
      IText vt(10.f, IColor(active ? 255 : dimA * 180 / 255, 255, 249, 235), kRobinMonoFont);
      vt.mAlign  = EAlign::Center;
      vt.mVAlign = EVAlign::Middle;
      g.DrawText(vt, valBuf, GetKnobValueRect(k));
    }
  }

  // -------------------------------------------------------------------------
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mSelectedKey < 0) return;
    int k = HitKnob(x, y);
    if (k < 0) return;
    mDragKnob     = k;
    mDragStartY   = y;
    mDragStartVal = mVals[mSelectedKey][kKnobs[k].valIdx];
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    mDragKnob = -1;
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if (mDragKnob < 0 || mSelectedKey < 0) return;
    const int   k     = mDragKnob;
    const float range = kKnobs[k].maxVal - kKnobs[k].minVal;
    float newVal = mDragStartVal + (mDragStartY - y) * range / kDragScale;
    newVal = std::max(kKnobs[k].minVal, std::min(kKnobs[k].maxVal, newVal));
    mVals[mSelectedKey][kKnobs[k].valIdx] = newVal;
    SendUpdate(mSelectedKey);
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSelectedKey < 0) return;
    int k = HitKnob(x, y);
    if (k < 0) return;
    mVals[mSelectedKey][kKnobs[k].valIdx] = kKnobs[k].defVal;
    mDragKnob = -1;
    SendUpdate(mSelectedKey);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    int k = (mSelectedKey >= 0) ? HitKnob(x, y) : -1;
    if (k != mHoveredKnob) { mHoveredKnob = k; SetDirty(false); }
  }

  void OnMouseOut() override
  {
    mHoveredKnob = mDragKnob = -1;
    SetDirty(false);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (msgTag == kMsgTagSampleLoaded && dataSize == sizeof(RobinStateMsg)) {
      const auto* m = static_cast<const RobinStateMsg*>(pData);
      if (m->note >= 0 && m->note < 128) {
        mHasSamples[m->note] = m->hasSamples;
        mCounts[m->note]     = m->sampleCount;
        strncpy(mLabels[m->note], m->label, 63);
        mLabels[m->note][63] = '\0';
        if (m->note == mSelectedKey) SetDirty(false);
      }
    }
    else if (msgTag == kMsgTagKeyParamSync && dataSize == sizeof(RobinKeyParamMsg)) {
      const auto* m = static_cast<const RobinKeyParamMsg*>(pData);
      if (m->note >= 0 && m->note < 128) {
        mVals[m->note][0] = m->gain;
        mVals[m->note][1] = m->pitch;
        mVals[m->note][2] = m->lead;
        mVals[m->note][3] = m->pan;
        if (m->note == mSelectedKey) SetDirty(false);
      }
    }
  }

private:
  // ---- Geometry helpers ---------------------------------------------------

  void GetKnobCenter(int k, float& cx, float& cy) const
  {
    const float areaL = mRECT.L + kInfoW;
    const float colW  = (mRECT.R - areaL) / kNumKnobs;
    cx = areaL + (k + 0.5f) * colW;
    cy = mRECT.T + 46.f;   // panel height 120 → knob center at T+46
  }

  IRECT GetKnobLabelRect(int k) const
  {
    const float areaL = mRECT.L + kInfoW;
    const float colW  = (mRECT.R - areaL) / kNumKnobs;
    const float colL  = areaL + k * colW;
    return IRECT(colL, mRECT.T + 80.f, colL + colW, mRECT.T + 94.f);
  }

  IRECT GetKnobValueRect(int k) const
  {
    const float areaL = mRECT.L + kInfoW;
    const float colW  = (mRECT.R - areaL) / kNumKnobs;
    const float colL  = areaL + k * colW;
    return IRECT(colL, mRECT.T + 96.f, colL + colW, mRECT.T + 110.f);
  }

  int HitKnob(float x, float y) const
  {
    for (int k = 0; k < kNumKnobs; k++) {
      float cx, cy;
      GetKnobCenter(k, cx, cy);
      const float dx = x - cx, dy = y - cy;
      if (dx*dx + dy*dy <= (kKnobR + 6.f) * (kKnobR + 6.f)) return k;
    }
    return -1;
  }

  // ---- Drawing ------------------------------------------------------------

  void DrawKnob(IGraphics& g, int k, float cx, float cy, float t,
                bool active, int dimA) const
  {
    const float startA  = kAngleStart;
    const float curA    = startA + t * kAngleSweep;
    const float centerA = kAngleCenter;

    // Subtle glow behind knob when active
    if (active) {
      const IColor& ac = kKnobArcColors[k];
      g.FillCircle(IColor(35, ac.R, ac.G, ac.B), cx, cy, kKnobR + 6.f);
    }

    // Arc track (full range, dim)
    g.DrawArc(IColor(int(85 * dimA / 255), 110, 110, 110),
              cx, cy, kArcR, startA, startA + kAngleSweep, nullptr, kArcThick);

    // Arc value (colored)
    const IColor& col = kKnobArcColors[k];
    const int arcA = active ? 255 : int(col.A * dimA / 255);
    IColor arcCol(arcA, col.R, col.G, col.B);

    if (kKnobs[k].bipolar) {
      if (t > 0.502f)
        g.DrawArc(arcCol, cx, cy, kArcR, centerA, curA, nullptr, kArcThick);
      else if (t < 0.498f)
        g.DrawArc(arcCol, cx, cy, kArcR, curA, centerA, nullptr, kArcThick);
      // center mark
      g.DrawLine(IColor(int(80 * dimA / 255), 200, 200, 200),
                 cx, cy - kBodyR * 0.9f, cx, cy - kBodyR * 0.55f, nullptr, 1.2f);
    } else {
      if (t > 0.005f)
        g.DrawArc(arcCol, cx, cy, kArcR, startA, curA, nullptr, kArcThick);
    }

    // Knob body
    const int bodyBrightness = active ? 72 : 58;
    g.FillCircle(IColor(255, bodyBrightness, bodyBrightness, bodyBrightness), cx, cy, kBodyR);
    g.DrawCircle(IColor(255, 85, 85, 85), cx, cy, kBodyR, nullptr, 1.f);

    // Indicator dot
    const float angleRad = curA * kRobinPI / 180.f;
    const float indDist  = kBodyR * 0.68f;
    const float nx = cx + indDist * std::cos(angleRad);
    const float ny = cy + indDist * std::sin(angleRad);
    g.FillCircle(IColor(int(235 * dimA / 255), 235, 235, 235), nx, ny, 3.8f);
    g.DrawCircle(IColor(int(60 * dimA / 255), 0, 0, 0), nx, ny, 3.8f, nullptr, 0.8f);
  }

  static void FormatVal(int k, float stored, char* buf, int sz)
  {
    const float d = stored * kKnobs[k].dispMult;
    if (k == 3) {      // Pan — show L/R
      if (std::abs(d) < 1.f) snprintf(buf, sz, "C");
      else if (d < 0)        snprintf(buf, sz, "L%.0f", -d);
      else                   snprintf(buf, sz, "R%.0f",  d);
    } else if (kKnobs[k].bipolar) {
      snprintf(buf, sz, "%+.0f%s", d, kKnobs[k].unit);
    } else {
      snprintf(buf, sz, "%.0f%s", d, kKnobs[k].unit);
    }
  }

  void SendUpdate(int note)
  {
    if (note < 0 || note >= 128) return;
    RobinKeyParamMsg m;
    m.note  = note;
    m.gain  = mVals[note][0];
    m.pitch = mVals[note][1];
    m.lead  = mVals[note][2];
    m.pan   = mVals[note][3];
    GetDelegate()->SendArbitraryMsgFromUI(kMsgTagSetKeyParam, GetTag(), sizeof(m), &m);
  }

  // ---- State --------------------------------------------------------------
  int   mSelectedKey  = -1;
  int   mDragKnob     = -1;
  int   mHoveredKnob  = -1;
  float mDragStartY   = 0.f;
  float mDragStartVal = 0.f;

  float mVals[128][kNumKnobs] {};   // stored units
  bool  mHasSamples[128]      {};
  int   mCounts[128]          {};
  char  mLabels[128][64]      {};
};

const IColor RobinPanelControl::kKnobArcColors[kNumKnobs] = {
  IColor(220, 202,  25,   9),   // Trim
  IColor(220, 202,  25,   9),   // Gain
  IColor(220, 202,  25,   9),   // Pitch
  IColor(220, 202,  25,   9),   // Pan
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
