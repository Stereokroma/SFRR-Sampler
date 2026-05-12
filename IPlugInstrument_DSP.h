#pragma once

#include "IPlugStructs.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <cstring>
#include <cmath>

#ifdef OS_WIN
  #include <windows.h>
  #define DR_WAV_IMPLEMENTATION
  #include "dr_wav.h"
#else
  #include <dirent.h>
  #include <sys/stat.h>
  #ifdef OS_MAC
    #include <AudioToolbox/ExtendedAudioFile.h>
    #include <CoreFoundation/CoreFoundation.h>
  #endif
#endif

using namespace iplug;

struct SampleData {
  std::vector<float> frames;
  int    numChannels = 0;
  double sampleRate  = 44100.0;
  int numFrames() const {
    return numChannels > 0 ? (int)(frames.size() / numChannels) : 0;
  }
};

using SampleDataPtr = std::shared_ptr<SampleData>;

struct KeyState {
  std::vector<SampleDataPtr> samples;
  std::string folderPath;
  int rrIndex    = 0;
  int lastPlayed = -1;

  float keyGain    = 1.0f;   // 0–2, unity = 1
  float keyPitch   = 0.0f;   // ±24 semitones
  float keyLead    = 0.0f;   // 0–1000 ms start offset
  float keyPan     = 0.0f;   // -1 (L) to +1 (R), 0 = center
  float keyAttack  = 0.0f;   // 0–1000 ms attack time
  float keyRelease = 500.0f; // 0–1000 ms release time

  int NextSampleIndex(bool randomMode) {
    int n = (int)samples.size();
    if (n == 0) return -1;
    if (n == 1) { lastPlayed = 0; return 0; }
    if (!randomMode) {
      int idx = rrIndex;
      rrIndex = (rrIndex + 1) % n;
      lastPlayed = idx;
      return idx;
    }
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, n - 1);
    int idx, tries = 0;
    do { idx = dist(rng); } while (idx == lastPlayed && ++tries < 20);
    lastPlayed = idx;
    return idx;
  }
};

struct SamplerVoice {
  SampleDataPtr pSample;
  int    note         = -1;
  double playPos      = 0.0;
  double rateRatio    = 1.0;
  float  velocity     = 1.0f;
  float  keyGain      = 1.0f;
  float  keyPan       = 0.0f;
  bool   active       = false;
  bool   releasing    = false;
  float  releaseGain  = 1.0f;
  float  relDelta     = 0.f;   // per-sample release decrement
  bool   inAttack     = false;
  float  attackGain   = 1.0f;
  float  attackDelta  = 0.f;   // per-sample attack increment
};

class RobinSamplerDSP
{
public:
  static constexpr int   kMaxVoices    = 64;
  // Used only for the retrigger choke (always a fast 20ms cut).
  static constexpr float kChokeDelta  = 1.f / (44100.f * 0.02f);

  RobinSamplerDSP() = default;

  void Reset(double sampleRate, int /*blockSize*/) { mSampleRate = sampleRate; }

  void ProcessBlock(sample** outputs, int nOutputs, int nFrames)
  {
    for (int ch = 0; ch < nOutputs; ch++)
      memset(outputs[ch], 0, nFrames * sizeof(sample));

    for (auto& v : mVoices) {
      if (!v.active || !v.pSample) continue;

      const SampleData& sd    = *v.pSample;
      const int         nCh   = sd.numChannels;
      const int         total = sd.numFrames();

      // Constant-power pan: keyPan -1=full-left, 0=center, +1=full-right
      const float panAngle = (v.keyPan + 1.f) * 0.25f * 3.14159265f;
      const float panL     = std::cos(panAngle);
      const float panR     = std::sin(panAngle);

      for (int i = 0; i < nFrames; i++) {
        if (v.inAttack) {
          v.attackGain += v.attackDelta;
          if (v.attackGain >= 1.f) { v.attackGain = 1.f; v.inAttack = false; }
        }

        if (v.releasing) {
          v.releaseGain -= v.relDelta;
          if (v.releaseGain <= 0.f) { v.active = false; break; }
        }

        int frame = (int)v.playPos;
        if (frame >= total) { v.active = false; break; }

        const double frac      = v.playPos - frame;
        const int    nextFrame = std::min(frame + 1, total - 1);
        const float  gain      = v.velocity * v.releaseGain * v.attackGain * v.keyGain;

        auto s = [&](int f, int c) -> float {
          return sd.frames[f * nCh + std::min(c, nCh - 1)];
        };

        if (nCh == 1) {
          float smp = (float)((1.0-frac)*s(frame,0) + frac*s(nextFrame,0)) * gain;
          if (nOutputs > 0) outputs[0][i] += smp * panL;
          if (nOutputs > 1) outputs[1][i] += smp * panR;
        } else {
          float ch0 = (float)((1.0-frac)*s(frame,0) + frac*s(nextFrame,0)) * gain;
          float ch1 = (float)((1.0-frac)*s(frame,1) + frac*s(nextFrame,1)) * gain;
          if (nOutputs > 0) outputs[0][i] += ch0 * panL;
          if (nOutputs > 1) outputs[1][i] += ch1 * panR;
        }

        v.playPos += v.rateRatio;
      }
    }
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    switch (msg.StatusMsg()) {
      case IMidiMsg::kNoteOn:
        if (msg.Velocity() > 0) TriggerNote(msg.NoteNumber(), msg.Velocity());
        else                     ReleaseNote(msg.NoteNumber());
        break;
      case IMidiMsg::kNoteOff: ReleaseNote(msg.NoteNumber()); break;
      default: break;
    }
  }

  void LoadSamplesForKey(int note, const std::string& path)
  {
    if (note < 0 || note > 127) return;
    std::vector<std::string> filePaths;
    CollectAudioFiles(path, filePaths);
    std::sort(filePaths.begin(), filePaths.end());

    std::vector<SampleDataPtr> loaded;
    for (const auto& fp : filePaths) {
      auto data = std::make_shared<SampleData>();
      if (LoadAudioFile(fp, *data)) loaded.push_back(std::move(data));
    }

    std::lock_guard<std::mutex> lock(mMutex);
    auto& key      = mKeys[note];
    key.samples    = std::move(loaded);
    key.folderPath = path;
    key.rrIndex    = 0;
    key.lastPlayed = -1;
  }

  void SetRandomMode(bool r)      { mGlobalRandom = r; }
  void SetVelocitySensitive(bool s) { mVelSensitive = s; }
  void SetSustainMode(bool s)     { mSustainMode  = s; }

  void SetKeyParams(int note, float gain, float pitch, float lead, float pan,
                    float attack, float release)
  {
    if (note < 0 || note > 127) return;
    std::lock_guard<std::mutex> lock(mMutex);
    mKeys[note].keyGain    = gain;
    mKeys[note].keyPitch   = pitch;
    mKeys[note].keyLead    = lead;
    mKeys[note].keyPan     = pan;
    mKeys[note].keyAttack  = attack;
    mKeys[note].keyRelease = release;
  }

  void GetKeyParams(int note, float& gain, float& pitch, float& lead, float& pan,
                    float& attack, float& release) const
  {
    if (note < 0 || note > 127) {
      gain=1.f; pitch=0.f; lead=0.f; pan=0.f; attack=0.f; release=500.f; return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    gain    = mKeys[note].keyGain;
    pitch   = mKeys[note].keyPitch;
    lead    = mKeys[note].keyLead;
    pan     = mKeys[note].keyPan;
    attack  = mKeys[note].keyAttack;
    release = mKeys[note].keyRelease;
  }

  void ClearAllKeys()
  {
    for (auto& v : mVoices) v.active = false;
    std::lock_guard<std::mutex> lock(mMutex);
    for (int n = 0; n < 128; n++) {
      mKeys[n].samples.clear();
      mKeys[n].folderPath.clear();
      mKeys[n].rrIndex    = 0;
      mKeys[n].lastPlayed = -1;
      mKeys[n].keyGain    = 1.0f;
      mKeys[n].keyPitch   = 0.0f;
      mKeys[n].keyLead    = 0.0f;
      mKeys[n].keyPan     = 0.0f;
      mKeys[n].keyAttack  = 0.0f;
      mKeys[n].keyRelease = 500.0f;
    }
  }

  bool HasSamples(int note) const
  {
    if (note < 0 || note > 127) return false;
    std::lock_guard<std::mutex> lock(mMutex);
    return !mKeys[note].samples.empty();
  }

  int GetSampleCount(int note) const
  {
    if (note < 0 || note > 127) return 0;
    std::lock_guard<std::mutex> lock(mMutex);
    return (int)mKeys[note].samples.size();
  }

  std::string GetFolderPath(int note) const
  {
    if (note < 0 || note > 127) return {};
    std::lock_guard<std::mutex> lock(mMutex);
    return mKeys[note].folderPath;
  }

  bool SerializeKeyStates(IByteChunk& chunk) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    for (int n = 0; n < 128; n++)
      chunk.PutStr(mKeys[n].folderPath.c_str());
    for (int n = 0; n < 128; n++) {
      chunk.Put<float>(&mKeys[n].keyGain);
      chunk.Put<float>(&mKeys[n].keyPitch);
      chunk.Put<float>(&mKeys[n].keyLead);
      chunk.Put<float>(&mKeys[n].keyPan);
      chunk.Put<float>(&mKeys[n].keyAttack);
      chunk.Put<float>(&mKeys[n].keyRelease);
    }
    return true;
  }

  int UnserializeKeyStates(const IByteChunk& chunk, int startPos)
  {
    for (int n = 0; n < 128; n++) {
      WDL_String path;
      startPos = chunk.GetStr(path, startPos);
      if (startPos < 0) return startPos;
      if (path.GetLength() > 0) LoadSamplesForKey(n, path.Get());
    }
    for (int n = 0; n < 128; n++) {
      float gain=1.f, pitch=0.f, lead=0.f, pan=0.f, attack=0.f, release=500.f;
      int np;
      np = chunk.Get<float>(&gain,    startPos); if (np < 0) break; startPos = np;
      np = chunk.Get<float>(&pitch,   startPos); if (np < 0) break; startPos = np;
      np = chunk.Get<float>(&lead,    startPos); if (np < 0) break; startPos = np;
      np = chunk.Get<float>(&pan,     startPos); if (np < 0) break; startPos = np;
      np = chunk.Get<float>(&attack,  startPos); if (np < 0) break; startPos = np;
      np = chunk.Get<float>(&release, startPos); if (np < 0) break; startPos = np;
      std::lock_guard<std::mutex> lock(mMutex);
      mKeys[n].keyGain    = gain;
      mKeys[n].keyPitch   = pitch;
      mKeys[n].keyLead    = lead;
      mKeys[n].keyPan     = pan;
      mKeys[n].keyAttack  = attack;
      mKeys[n].keyRelease = release;
    }
    return startPos;
  }

private:
  void TriggerNote(int note, int velocity)
  {
    float kGain, kPitch, kLead, kPan, kAttack, kRelease;
    SampleDataPtr sample;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      auto& key = mKeys[note];
      if (key.samples.empty()) return;
      int idx = key.NextSampleIndex(mGlobalRandom);
      if (idx < 0) return;
      sample   = key.samples[idx];
      kGain    = key.keyGain;
      kPitch   = key.keyPitch;
      kLead    = key.keyLead;
      kPan     = key.keyPan;
      kAttack  = key.keyAttack;
      kRelease = key.keyRelease;
    }

    const float chokeDelta = (float)(kChokeDelta * 44100.0 / mSampleRate);
    if (!mSustainMode) {
      for (auto& v : mVoices)
        if (v.active && v.note == note && !v.releasing) {
          v.releasing   = true;
          v.inAttack    = false;
          v.attackGain  = 1.f;
          v.releaseGain = 1.f;
          v.relDelta    = chokeDelta;
        }
    }

    SamplerVoice* v = FindFreeVoice(note, (bool)mSustainMode);
    if (!v) return;

    v->note        = note;
    v->pSample     = sample;
    v->playPos     = (kLead / 1000.0) * sample->sampleRate;
    v->rateRatio   = (sample->sampleRate / mSampleRate) * std::pow(2.0, kPitch / 12.0);
    v->velocity    = mVelSensitive ? velocity / 127.f : 1.0f;
    v->keyGain     = kGain;
    v->keyPan      = kPan;
    v->active      = true;
    v->releasing   = false;
    v->releaseGain = 1.f;
    v->relDelta    = (kRelease > 0.f)
                       ? 1.f / (float)(mSampleRate * kRelease / 1000.f)
                       : 1.f;
    if (kAttack > 0.f) {
      v->inAttack    = true;
      v->attackGain  = 0.f;
      v->attackDelta = 1.f / (float)(mSampleRate * kAttack / 1000.f);
    } else {
      v->inAttack    = false;
      v->attackGain  = 1.f;
      v->attackDelta = 0.f;
    }
  }

  void ReleaseNote(int note)
  {
    if (mSustainMode) return;
    for (auto& v : mVoices)
      if (v.active && v.note == note && !v.releasing)
        v.releasing = true;
    // relDelta was already set at trigger time, so the per-key release time is applied
  }

  SamplerVoice* FindFreeVoice(int note, bool sustainMode)
  {
    // Always prefer a completely idle voice
    for (auto& v : mVoices) if (!v.active) return &v;

    if (sustainMode) {
      // Sustain ON: never steal a releasing voice — let it play to completion.
      // If all voices are busy, steal the one furthest through its sample
      // (closest to finishing naturally).
      SamplerVoice* best    = nullptr;
      float          maxProg = -1.f;
      for (auto& v : mVoices) {
        if (!v.pSample) continue;
        const int   tot  = v.pSample->numFrames();
        const float prog = (tot > 0) ? (float)(v.playPos / (double)tot) : 1.f;
        if (prog > maxProg) { maxProg = prog; best = &v; }
      }
      return best ? best : &mVoices[0];
    }

    // Sustain OFF: steal releasing voices (same note first, then quietest)
    for (auto& v : mVoices) if (v.note == note && v.releasing) return &v;
    SamplerVoice* q = nullptr;
    for (auto& v : mVoices)
      if (v.releasing && (!q || v.releaseGain < q->releaseGain)) q = &v;
    if (q) return q;

    // Last resort: steal the voice furthest through its sample (least audible)
    SamplerVoice* best    = nullptr;
    float          maxProg = -1.f;
    for (auto& v : mVoices) {
      if (!v.pSample) continue;
      const int   tot  = v.pSample->numFrames();
      const float prog = (tot > 0) ? (float)(v.playPos / (double)tot) : 1.f;
      if (prog > maxProg) { maxProg = prog; best = &v; }
    }
    return best ? best : &mVoices[0];
  }

  static bool IsAudioFile(const std::string& p)
  {
    auto dot = p.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = p.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "wav" || ext == "aif" || ext == "aiff";
  }

  static bool IsDirectory(const std::string& p)
  {
#ifdef OS_WIN
    DWORD attrs = GetFileAttributesA(p.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
  }

  static void CollectAudioFiles(const std::string& pathIn, std::vector<std::string>& out)
  {
    if (IsDirectory(pathIn)) {
#ifdef OS_WIN
      WIN32_FIND_DATAA ffd;
      HANDLE hFind = FindFirstFileA((pathIn + "\\*").c_str(), &ffd);
      if (hFind == INVALID_HANDLE_VALUE) return;
      do {
        std::string name = ffd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = pathIn + "\\" + name;
        if (IsAudioFile(full)) out.push_back(full);
      } while (FindNextFileA(hFind, &ffd));
      FindClose(hFind);
#else
      DIR* dir = opendir(pathIn.c_str());
      if (!dir) return;
      struct dirent* e;
      while ((e = readdir(dir))) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string full = pathIn + "/" + name;
        if (IsAudioFile(full)) out.push_back(full);
      }
      closedir(dir);
#endif
    } else if (IsAudioFile(pathIn)) {
      out.push_back(pathIn);
    }
  }

#ifdef OS_WIN
  static bool LoadAudioFile(const std::string& path, SampleData& out)
  {
    unsigned int channels, sampleRate;
    drwav_uint64 totalFrames;
    float* pData = drwav_open_file_and_read_pcm_frames_f32(
      path.c_str(), &channels, &sampleRate, &totalFrames, nullptr);
    if (!pData || totalFrames == 0) { drwav_free(pData, nullptr); return false; }

    out.sampleRate  = sampleRate;
    out.numChannels = std::min((int)channels, 2);
    out.frames.resize((size_t)totalFrames * out.numChannels);

    for (drwav_uint64 i = 0; i < totalFrames; i++)
      for (int ch = 0; ch < out.numChannels; ch++)
        out.frames[i * out.numChannels + ch] = pData[i * channels + ch];

    drwav_free(pData, nullptr);
    return true;
  }
#elif defined OS_MAC
  static bool LoadAudioFile(const std::string& path, SampleData& out)
  {
    CFStringRef cfStr = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    if (!cfStr) return false;
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, cfStr, kCFURLPOSIXPathStyle, false);
    CFRelease(cfStr);
    if (!url) return false;

    ExtAudioFileRef fileRef = nullptr;
    OSStatus err = ExtAudioFileOpenURL(url, &fileRef);
    CFRelease(url);
    if (err != noErr || !fileRef) return false;

    AudioStreamBasicDescription native = {};
    UInt32 propSize = sizeof(native);
    ExtAudioFileGetProperty(fileRef, kExtAudioFileProperty_FileDataFormat, &propSize, &native);
    out.sampleRate  = native.mSampleRate;
    out.numChannels = std::min((int)native.mChannelsPerFrame, 2);

    AudioStreamBasicDescription client = {};
    client.mSampleRate       = native.mSampleRate;
    client.mFormatID         = kAudioFormatLinearPCM;
    client.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    client.mBitsPerChannel   = 32;
    client.mChannelsPerFrame = (UInt32)out.numChannels;
    client.mBytesPerFrame    = sizeof(float) * (UInt32)out.numChannels;
    client.mFramesPerPacket  = 1;
    client.mBytesPerPacket   = client.mBytesPerFrame;

    err = ExtAudioFileSetProperty(fileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(client), &client);
    if (err != noErr) { ExtAudioFileDispose(fileRef); return false; }

    SInt64 nFrames = 0;
    propSize = sizeof(nFrames);
    ExtAudioFileGetProperty(fileRef, kExtAudioFileProperty_FileLengthFrames, &propSize, &nFrames);
    if (nFrames <= 0) { ExtAudioFileDispose(fileRef); return false; }

    out.frames.resize((size_t)nFrames * out.numChannels);
    AudioBufferList buf;
    buf.mNumberBuffers              = 1;
    buf.mBuffers[0].mNumberChannels = (UInt32)out.numChannels;
    buf.mBuffers[0].mDataByteSize   = (UInt32)(out.frames.size() * sizeof(float));
    buf.mBuffers[0].mData           = out.frames.data();

    UInt32 framesRead = (UInt32)nFrames;
    err = ExtAudioFileRead(fileRef, &framesRead, &buf);
    out.frames.resize((size_t)framesRead * out.numChannels);
    ExtAudioFileDispose(fileRef);
    return err == noErr && framesRead > 0;
  }
#else
  static bool LoadAudioFile(const std::string&, SampleData&) { return false; }
#endif

  std::array<KeyState, 128>            mKeys   {};
  std::array<SamplerVoice, kMaxVoices> mVoices {};
  mutable std::mutex                   mMutex;
  double                               mSampleRate   = 44100.0;
  std::atomic<bool>                    mGlobalRandom {false};
  std::atomic<bool>                    mVelSensitive {true};
  std::atomic<bool>                    mSustainMode  {false};
};
