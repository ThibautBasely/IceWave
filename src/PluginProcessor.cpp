#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace icewave;

namespace
{
using Attr = juce::AudioParameterFloatAttributes;

juce::String pct (float v, int)    { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
juce::String hz (float v, int)     { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }
juce::String secs (float v, int)   { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; }
juce::String db (float v, int)     { return juce::String (v, 1) + " dB"; }
juce::String lfoHz (float v, int)  { return juce::String (v, 2) + " Hz"; }

std::unique_ptr<juce::AudioParameterFloat> fl (const juce::String& id, const juce::String& name,
                                               juce::NormalisableRange<float> r, float def,
                                               juce::String (*fn) (float, int))
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, Attr().withStringFromValueFunction (fn));
}

juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout IceWaveProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    juce::StringArray tables;
    for (int i = 0; i < kNumTables; ++i) tables.add (WavetableBank::tableName (i));

    const char* ids[2] = { "oscA_", "oscB_" };
    const char* names[2] = { "Osc A ", "Osc B " };
    for (int o = 0; o < 2; ++o)
    {
        juce::String p (ids[o]), n (names[o]);
        L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { p + "on", 1 }, n + "On", o == 0));
        L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { p + "table", 1 }, n + "Table", tables, 0));
        L.add (fl (p + "pos", n + "Position", { 0.0f, 1.0f }, o == 0 ? 0.67f : 0.0f, pct));
        L.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { p + "semi", 1 }, n + "Semi", -24, 24, 0,
               juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; })));
        L.add (fl (p + "fine", n + "Fine", { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; }));
        L.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { p + "uni", 1 }, n + "Unison", 1, 7, 1));
        L.add (fl (p + "detune", n + "Detune", { 0.0f, 1.0f }, 0.2f, pct));
        L.add (fl (p + "level", n + "Level", { 0.0f, 1.0f }, 0.75f, pct));
    }

    L.add (fl ("sub_level", "Sub Level", { 0.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("noise_level", "Noise Level", { 0.0f, 1.0f }, 0.0f, pct));

    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "flt_type", 1 }, "Filter Type", juce::StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));
    L.add (fl ("flt_cutoff", "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hz));
    L.add (fl ("flt_reso", "Resonance", { 0.0f, 1.0f }, 0.1f, pct));
    L.add (fl ("flt_drive", "Drive", { 0.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("flt_env", "Filter Env", { -1.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("flt_key", "Key Track", { 0.0f, 1.0f }, 0.0f, pct));

    L.add (fl ("env1_a", "Amp Attack", skewed (0.001f, 10.0f, 0.5f), 0.005f, secs));
    L.add (fl ("env1_d", "Amp Decay", skewed (0.001f, 10.0f, 0.5f), 0.4f, secs));
    L.add (fl ("env1_s", "Amp Sustain", { 0.0f, 1.0f }, 0.8f, pct));
    L.add (fl ("env1_r", "Amp Release", skewed (0.001f, 10.0f, 0.5f), 0.25f, secs));

    L.add (fl ("env2_a", "Mod Attack", skewed (0.001f, 10.0f, 0.5f), 0.005f, secs));
    L.add (fl ("env2_d", "Mod Decay", skewed (0.001f, 10.0f, 0.5f), 0.5f, secs));
    L.add (fl ("env2_s", "Mod Sustain", { 0.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("env2_r", "Mod Release", skewed (0.001f, 10.0f, 0.5f), 0.3f, secs));
    L.add (fl ("env2_pos", "Mod Env > Position", { -1.0f, 1.0f }, 0.0f, pct));

    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "lfo_shape", 1 }, "LFO Shape", juce::StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));
    L.add (fl ("lfo_rate", "LFO Rate", skewed (0.05f, 20.0f, 2.0f), 1.0f, lfoHz));
    L.add (fl ("lfo_pos", "LFO > Position", { 0.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("lfo_cut", "LFO > Cutoff", { 0.0f, 1.0f }, 0.0f, pct));
    L.add (fl ("lfo_pitch", "LFO > Pitch", { 0.0f, 1.0f }, 0.0f, pct));

    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "dist_on", 1 }, "Distortion On", false));
    L.add (fl ("dist_drive", "Dist Drive", { 0.0f, 1.0f }, 0.3f, pct));
    L.add (fl ("dist_mix", "Dist Mix", { 0.0f, 1.0f }, 0.5f, pct));

    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "cho_on", 1 }, "Chorus On", false));
    L.add (fl ("cho_rate", "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, lfoHz));
    L.add (fl ("cho_depth", "Chorus Depth", { 0.0f, 1.0f }, 0.4f, pct));
    L.add (fl ("cho_mix", "Chorus Mix", { 0.0f, 1.0f }, 0.4f, pct));

    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "dly_on", 1 }, "Delay On", false));
    L.add (fl ("dly_time", "Delay Time", skewed (0.02f, 1.5f, 0.35f), 0.375f, secs));
    L.add (fl ("dly_fb", "Delay Feedback", { 0.0f, 0.95f }, 0.35f, pct));
    L.add (fl ("dly_mix", "Delay Mix", { 0.0f, 1.0f }, 0.25f, pct));

    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "rev_on", 1 }, "Reverb On", false));
    L.add (fl ("rev_size", "Reverb Size", { 0.0f, 1.0f }, 0.6f, pct));
    L.add (fl ("rev_damp", "Reverb Damping", { 0.0f, 1.0f }, 0.4f, pct));
    L.add (fl ("rev_mix", "Reverb Mix", { 0.0f, 1.0f }, 0.25f, pct));

    L.add (fl ("master", "Master", { -36.0f, 6.0f }, -3.0f, db));
    return L;
}

IceWaveProcessor::IceWaveProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IceWave", createLayout())
{
    WavetableBank::get();
    refs.init (apvts);
    synth.addSound (new IceSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new IceVoice (refs));
}

bool IceWaveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void IceWaveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<IceVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    chorus.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    chorus.reset();
    reverb.setSampleRate (sampleRate);
    reverb.reset();

    delayBuf.setSize (2, (int) (sampleRate * 2.0) + 4);
    delayBuf.clear();
    delayWrite = 0;
    delaySamples.reset (sampleRate, 0.15);
    delaySamples.setCurrentAndTargetValue (apvts.getRawParameterValue ("dly_time")->load() * (float) sampleRate);
    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("master")->load()));
    keyboardState.reset();
}

void IceWaveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, n);

    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // Distortion
    if (v ("dist_on") > 0.5f)
    {
        const float k = 1.0f + v ("dist_drive") * 20.0f;
        const float norm = 1.0f / std::tanh (k);
        const float mix = v ("dist_mix");
        for (int i = 0; i < n; ++i)
        {
            L[i] += (std::tanh (L[i] * k) * norm - L[i]) * mix;
            R[i] += (std::tanh (R[i] * k) * norm - R[i]) * mix;
        }
    }

    // Chorus
    if (v ("cho_on") > 0.5f)
    {
        chorus.setRate (v ("cho_rate"));
        chorus.setDepth (v ("cho_depth"));
        chorus.setCentreDelay (8.0f);
        chorus.setFeedback (0.1f);
        chorus.setMix (v ("cho_mix") * 0.5f);
        juce::dsp::AudioBlock<float> block (buffer);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    // Ping-pong delay
    const bool dlyOn = v ("dly_on") > 0.5f;
    if (dlyOn)
    {
        delaySamples.setTargetValue (v ("dly_time") * (float) sr);
        const float fb = v ("dly_fb"), mix = v ("dly_mix");
        const int size = delayBuf.getNumSamples();
        float* dL = delayBuf.getWritePointer (0);
        float* dR = delayBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            float rp = (float) delayWrite - delaySamples.getNextValue();
            if (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float fr = rp - (float) i0;
            const float yL = dL[i0] + (dL[i1] - dL[i0]) * fr;
            const float yR = dR[i0] + (dR[i1] - dR[i0]) * fr;
            dL[delayWrite] = L[i] + fb * yR;
            dR[delayWrite] = R[i] + fb * yL;
            L[i] = L[i] * (1.0f - mix * 0.3f) + yL * mix;
            R[i] = R[i] * (1.0f - mix * 0.3f) + yR * mix;
            if (++delayWrite >= size) delayWrite = 0;
        }
    }
    else if (delayWasOn)
    {
        delayBuf.clear();
    }
    delayWasOn = dlyOn;

    // Reverb
    if (v ("rev_on") > 0.5f)
    {
        juce::Reverb::Parameters p;
        const float mix = v ("rev_mix");
        p.roomSize = v ("rev_size");
        p.damping = v ("rev_damp");
        p.wetLevel = mix * 0.33f;
        p.dryLevel = 0.5f * (1.0f - mix * 0.5f);
        p.width = 1.0f;
        reverb.setParameters (p);
        reverb.processStereo (L, R, n);
    }

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (v ("master")));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}

juce::AudioProcessorEditor* IceWaveProcessor::createEditor() { return new IceWaveEditor (*this); }

void IceWaveProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void IceWaveProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ----------------------------------------------------------------- Presets
namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Init", {} },
        { "Supersaw Lead", { { "oscA_uni", 7 }, { "oscA_detune", 0.35f }, { "oscB_on", 1 }, { "oscB_pos", 0.67f }, { "oscB_semi", 12 },
                             { "oscB_uni", 5 }, { "oscB_detune", 0.25f }, { "oscB_level", 0.4f }, { "flt_cutoff", 6000 }, { "flt_reso", 0.2f },
                             { "env1_s", 0.8f }, { "env1_r", 0.3f }, { "rev_on", 1 }, { "rev_mix", 0.25f }, { "dly_on", 1 }, { "dly_mix", 0.2f } } },
        { "Deep Sub Bass", { { "oscA_pos", 0.4f }, { "oscA_level", 0.6f }, { "sub_level", 0.8f }, { "flt_cutoff", 350 }, { "flt_reso", 0.3f },
                             { "flt_env", 0.35f }, { "env2_d", 0.25f }, { "env1_r", 0.1f }, { "dist_on", 1 }, { "dist_drive", 0.3f }, { "dist_mix", 0.4f } } },
        { "Reese Bass", { { "oscA_uni", 2 }, { "oscA_detune", 0.25f }, { "oscB_on", 1 }, { "oscB_pos", 0.67f }, { "oscB_semi", -12 }, { "oscB_uni", 2 },
                          { "oscB_detune", 0.2f }, { "flt_cutoff", 900 }, { "flt_reso", 0.25f }, { "flt_drive", 0.4f }, { "lfo_rate", 0.3f },
                          { "lfo_cut", 0.2f }, { "cho_on", 1 }, { "env1_r", 0.15f } } },
        { "Vocal Pad", { { "oscA_table", 3 }, { "oscA_pos", 0.5f }, { "oscA_uni", 3 }, { "oscA_detune", 0.15f }, { "oscB_on", 1 }, { "oscB_table", 3 },
                         { "oscB_pos", 0.2f }, { "oscB_semi", 7 }, { "oscB_level", 0.5f }, { "lfo_rate", 0.25f }, { "lfo_pos", 0.8f },
                         { "env1_a", 1.2f }, { "env1_s", 1.0f }, { "env1_r", 2.5f }, { "flt_cutoff", 4000 }, { "cho_on", 1 },
                         { "rev_on", 1 }, { "rev_size", 0.85f }, { "rev_mix", 0.45f } } },
        { "PWM Strings", { { "oscA_table", 1 }, { "oscA_pos", 0.3f }, { "oscA_uni", 3 }, { "oscA_detune", 0.15f }, { "lfo_rate", 0.8f }, { "lfo_pos", 0.7f },
                           { "env1_a", 0.4f }, { "env1_s", 0.9f }, { "env1_r", 1.2f }, { "flt_cutoff", 5000 }, { "cho_on", 1 }, { "cho_mix", 0.5f },
                           { "rev_on", 1 }, { "rev_mix", 0.3f } } },
        { "Pluck", { { "oscB_on", 1 }, { "oscB_pos", 1.0f }, { "oscB_semi", 12 }, { "oscB_level", 0.3f }, { "flt_cutoff", 300 }, { "flt_reso", 0.35f },
                     { "flt_env", 0.7f }, { "env2_d", 0.35f }, { "env1_d", 0.6f }, { "env1_s", 0.0f }, { "env1_r", 0.4f },
                     { "dly_on", 1 }, { "dly_fb", 0.3f }, { "dly_mix", 0.25f }, { "rev_on", 1 }, { "rev_mix", 0.2f } } },
        { "Wobble Bass", { { "oscA_table", 2 }, { "oscA_pos", 0.3f }, { "sub_level", 0.6f }, { "flt_cutoff", 600 }, { "flt_reso", 0.45f },
                           { "lfo_rate", 3.0f }, { "lfo_cut", 0.7f }, { "lfo_pos", 0.4f }, { "dist_on", 1 }, { "dist_drive", 0.5f }, { "env1_r", 0.12f } } },
        { "Harmonic Bell", { { "oscA_table", 2 }, { "oscA_pos", 0.1f }, { "env2_pos", 0.6f }, { "env2_d", 1.0f }, { "env1_a", 0.001f }, { "env1_d", 2.0f },
                             { "env1_s", 0.0f }, { "env1_r", 1.5f }, { "oscB_on", 1 }, { "oscB_table", 2 }, { "oscB_pos", 0.5f }, { "oscB_semi", 19 },
                             { "oscB_level", 0.3f }, { "rev_on", 1 }, { "rev_mix", 0.35f }, { "dly_on", 1 }, { "dly_mix", 0.2f } } },
    };
    return p;
}
} // namespace

juce::StringArray IceWaveProcessor::getPresetNames()
{
    juce::StringArray s;
    for (auto& p : presets()) s.add (p.name);
    return s;
}

void IceWaveProcessor::loadPreset (int index)
{
    const auto& all = presets();
    if (index < 0 || index >= (int) all.size()) return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, value] : all[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    apvts.state.setProperty ("preset", index, nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IceWaveProcessor(); }
