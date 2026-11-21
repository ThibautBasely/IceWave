#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Wavetables.h"

namespace icewave
{
struct OscRefs
{
    std::atomic<float> *on = nullptr, *table = nullptr, *pos = nullptr, *semi = nullptr,
                       *fine = nullptr, *uni = nullptr, *detune = nullptr, *level = nullptr;
};

struct ParamRefs
{
    OscRefs osc[2];
    std::atomic<float> *subLevel = nullptr, *noiseLevel = nullptr;
    std::atomic<float> *fltType = nullptr, *cutoff = nullptr, *reso = nullptr, *drive = nullptr,
                       *fltEnv = nullptr, *fltKey = nullptr;
    std::atomic<float> *a1 = nullptr, *d1 = nullptr, *s1 = nullptr, *r1 = nullptr;
    std::atomic<float> *a2 = nullptr, *d2 = nullptr, *s2 = nullptr, *r2 = nullptr, *env2Pos = nullptr;
    std::atomic<float> *lfoRate = nullptr, *lfoShape = nullptr, *lfoPos = nullptr,
                       *lfoCut = nullptr, *lfoPitch = nullptr;

    void init (juce::AudioProcessorValueTreeState& apvts);
};

struct IceSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class IceVoice : public juce::SynthesiserVoice
{
public:
    explicit IceVoice (const ParamRefs& p) : P (p) {}

    void prepare (double sampleRate);
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<IceSound*> (s) != nullptr; }
    void startNote (int note, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int value) override;
    void controllerMoved (int controller, int value) override;
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;

private:
    static constexpr int kMaxUni = 7;
    static constexpr int kCtrl = 16;

    struct Osc
    {
        bool active = false;
        int uni = 1;
        const float* p0 = nullptr;
        const float* p1 = nullptr;
        float frac = 0.0f;
        double phase[kMaxUni] {};
        double inc[kMaxUni] {};
        float gL[kMaxUni] {}, gR[kMaxUni] {};
    };

    void updateEnvParams();
    void updateControl();
    float lfoValue() const;

    const ParamRefs& P;
    double sr = 44100.0;
    int note = 60;
    float velGain = 1.0f;
    float bendSemis = 0.0f;
    float modWheel = 0.0f;

    Osc osc[2];
    double subPhase = 0.0, subInc = 0.0;
    float subLvl = 0.0f, noiseLvl = 0.0f;

    juce::ADSR ampEnv, modEnv;
    float modEnvLast = 0.0f;
    double lfoPhase = 0.0;
    int ctrlCounter = 0;

    int fltType = 0;
    float fa1 = 0, fa2 = 0, fa3 = 0, fk = 2;
    float ic1[2] {}, ic2[2] {};
    float drivePre = 1.0f;
    bool driveOn = false;

    juce::Random rng;
};
} // namespace icewave
