#include "SynthVoice.h"
#include <cmath>

namespace icewave
{
static constexpr float kPi = 3.14159265358979f;

void ParamRefs::init (juce::AudioProcessorValueTreeState& a)
{
    auto g = [&a] (const juce::String& id) { return a.getRawParameterValue (id); };
    const char* pre[2] = { "oscA_", "oscB_" };
    for (int o = 0; o < 2; ++o)
    {
        juce::String p (pre[o]);
        osc[o].on = g (p + "on");       osc[o].table = g (p + "table");
        osc[o].pos = g (p + "pos");     osc[o].semi = g (p + "semi");
        osc[o].fine = g (p + "fine");   osc[o].uni = g (p + "uni");
        osc[o].detune = g (p + "detune"); osc[o].level = g (p + "level");
    }
    subLevel = g ("sub_level"); noiseLevel = g ("noise_level");
    fltType = g ("flt_type"); cutoff = g ("flt_cutoff"); reso = g ("flt_reso");
    drive = g ("flt_drive"); fltEnv = g ("flt_env"); fltKey = g ("flt_key");
    a1 = g ("env1_a"); d1 = g ("env1_d"); s1 = g ("env1_s"); r1 = g ("env1_r");
    a2 = g ("env2_a"); d2 = g ("env2_d"); s2 = g ("env2_s"); r2 = g ("env2_r");
    env2Pos = g ("env2_pos");
    lfoRate = g ("lfo_rate"); lfoShape = g ("lfo_shape"); lfoPos = g ("lfo_pos");
    lfoCut = g ("lfo_cut"); lfoPitch = g ("lfo_pitch");
}

void IceVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    ampEnv.setSampleRate (sr);
    modEnv.setSampleRate (sr);
}

void IceVoice::updateEnvParams()
{
    ampEnv.setParameters ({ P.a1->load(), P.d1->load(), P.s1->load(), P.r1->load() });
    modEnv.setParameters ({ P.a2->load(), P.d2->load(), P.s2->load(), P.r2->load() });
}

void IceVoice::startNote (int n, float velocity, juce::SynthesiserSound*, int pitchWheel)
{
    note = n;
    velGain = 0.3f + 0.7f * velocity;
    pitchWheelMoved (pitchWheel);

    for (auto& o : osc)
    {
        o.phase[0] = 0.0;
        for (int k = 1; k < kMaxUni; ++k)
            o.phase[k] = rng.nextDouble();
    }
    subPhase = 0.0;
    lfoPhase = 0.0;
    modEnvLast = 0.0f;
    ctrlCounter = 0;
    for (int c = 0; c < 2; ++c) ic1[c] = ic2[c] = 0.0f;

    updateEnvParams();
    ampEnv.reset();
    modEnv.reset();
    ampEnv.noteOn();
    modEnv.noteOn();
}

void IceVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        modEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        modEnv.reset();
        clearCurrentNote();
    }
}

void IceVoice::pitchWheelMoved (int value)
{
    bendSemis = 2.0f * (float) (value - 8192) / 8192.0f;
}

void IceVoice::controllerMoved (int controller, int value)
{
    if (controller == 1)
        modWheel = (float) value / 127.0f;
}

float IceVoice::lfoValue() const
{
    const float ph = (float) lfoPhase;
    switch ((int) P.lfoShape->load())
    {
        case 0:  return std::sin (2.0f * kPi * ph);
        case 1:  return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
        case 2:  return 1.0f - 2.0f * ph;
        default: return ph < 0.5f ? 1.0f : -1.0f;
    }
}

static inline double midiToHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

void IceVoice::updateControl()
{
    const auto& bank = WavetableBank::get();
    const float lfo = lfoValue();
    lfoPhase += P.lfoRate->load() * kCtrl / sr;
    lfoPhase -= std::floor (lfoPhase);

    const float env2 = modEnvLast;
    const float posMod = lfo * P.lfoPos->load() * 0.5f + env2 * P.env2Pos->load();
    const float pitchMod = bendSemis + lfo * (P.lfoPitch->load() + modWheel * 0.5f);

    for (int o = 0; o < 2; ++o)
    {
        auto& st = osc[o];
        const auto& R = P.osc[o];
        st.active = R.on->load() > 0.5f && R.level->load() > 0.0001f;
        if (! st.active) continue;

        const int table = juce::jlimit (0, kNumTables - 1, (int) R.table->load());
        const float pos = juce::jlimit (0.0f, 1.0f, R.pos->load() + posMod);
        const float fp = pos * (kFrames - 1);
        const int f0 = std::min ((int) fp, kFrames - 1);
        const int f1 = std::min (f0 + 1, kFrames - 1);
        st.frac = fp - (float) f0;

        st.uni = juce::jlimit (1, kMaxUni, (int) std::lround (R.uni->load()));
        const float detune = R.detune->load();
        const double base = note + R.semi->load() + R.fine->load() / 100.0 + pitchMod;

        const int mip = WavetableBank::mipFor (midiToHz (base + (st.uni > 1 ? detune * 0.5 : 0.0)), sr);
        st.p0 = bank.frame (table, f0, mip);
        st.p1 = bank.frame (table, f1, mip);

        const float lvl = R.level->load() / std::sqrt ((float) st.uni) * 1.41421356f;
        for (int k = 0; k < st.uni; ++k)
        {
            const float spread = st.uni > 1 ? 2.0f * (float) k / (float) (st.uni - 1) - 1.0f : 0.0f;
            st.inc[k] = midiToHz (base + detune * 0.5f * spread) / sr;
            const float angle = (0.8f * spread + 1.0f) * kPi * 0.25f;
            st.gL[k] = lvl * std::cos (angle);
            st.gR[k] = lvl * std::sin (angle);
        }
    }

    subLvl = P.subLevel->load();
    noiseLvl = P.noiseLevel->load();
    subInc = midiToHz (note - 12 + pitchMod) / sr;

    // filter
    fltType = (int) P.fltType->load();
    const float oct = P.fltEnv->load() * 5.0f * env2
                    + lfo * P.lfoCut->load() * 3.0f
                    + P.fltKey->load() * (float) (note - 60) / 12.0f;
    const float fc = juce::jlimit (20.0f, (float) (0.45 * sr), P.cutoff->load() * std::pow (2.0f, oct));
    const float g = std::tan (kPi * fc / (float) sr);
    fk = 2.0f - 1.96f * P.reso->load();
    fa1 = 1.0f / (1.0f + g * (g + fk));
    fa2 = g * fa1;
    fa3 = g * fa2;

    const float drv = P.drive->load();
    driveOn = drv > 0.001f;
    drivePre = 1.0f + drv * 8.0f;
}

void IceVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isVoiceActive()) return;
    updateEnvParams();

    float* L = out.getWritePointer (0, start);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

    int i = 0;
    while (i < num)
    {
        if (ctrlCounter <= 0)
        {
            updateControl();
            ctrlCounter = kCtrl;
        }
        const int chunk = std::min (num - i, ctrlCounter);

        for (int j = 0; j < chunk; ++j, ++i)
        {
            float l = 0.0f, r = 0.0f;

            for (auto& st : osc)
            {
                if (! st.active) continue;
                for (int k = 0; k < st.uni; ++k)
                {
                    const double ph = st.phase[k] * kTableSize;
                    const int i0 = (int) ph;
                    const float fr = (float) (ph - i0);
                    const float a = st.p0[i0] + (st.p0[i0 + 1] - st.p0[i0]) * fr;
                    const float b = st.p1[i0] + (st.p1[i0 + 1] - st.p1[i0]) * fr;
                    const float s = a + (b - a) * st.frac;
                    l += s * st.gL[k];
                    r += s * st.gR[k];
                    double np = st.phase[k] + st.inc[k];
                    st.phase[k] = np >= 1.0 ? np - 1.0 : np;
                }
            }

            if (subLvl > 0.0001f)
            {
                const float s = std::sin (2.0f * kPi * (float) subPhase) * subLvl;
                l += s; r += s;
            }
            subPhase += subInc;
            if (subPhase >= 1.0) subPhase -= 1.0;

            if (noiseLvl > 0.0001f)
            {
                const float s = (rng.nextFloat() * 2.0f - 1.0f) * noiseLvl;
                l += s; r += s;
            }

            float in[2] = { l * 0.5f, r * 0.5f };
            float outS[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = in[c];
                if (driveOn) x = std::tanh (x * drivePre);
                const float v3 = x - ic2[c];
                const float v1 = fa1 * ic1[c] + fa2 * v3;
                const float v2 = ic2[c] + fa2 * ic1[c] + fa3 * v3;
                ic1[c] = 2.0f * v1 - ic1[c];
                ic2[c] = 2.0f * v2 - ic2[c];
                outS[c] = fltType == 0 ? v2 : (fltType == 1 ? v1 : x - fk * v1 - v2);
            }

            modEnvLast = modEnv.getNextSample();
            const float amp = ampEnv.getNextSample() * velGain * 0.5f;
            L[i] += outS[0] * amp;
            if (R != nullptr) R[i] += outS[1] * amp;
        }
        ctrlCounter -= chunk;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
} // namespace icewave
