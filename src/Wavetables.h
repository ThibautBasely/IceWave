#pragma once
#include <vector>
#include <algorithm>

namespace icewave
{
constexpr int kTableSize  = 2048;
constexpr int kFrames     = 32;
constexpr int kMips       = 11;   // 1024, 512, ... 1 harmonics
constexpr int kNumTables  = 4;

class WavetableBank
{
public:
    static const WavetableBank& get()
    {
        static WavetableBank bank;
        return bank;
    }

    static const char* tableName (int i);

    const float* frame (int table, int frameIndex, int mip) const
    {
        return data[table].data() + ((size_t) frameIndex * kMips + (size_t) mip) * (kTableSize + 1);
    }

    static int mipFor (double freq, double sampleRate)
    {
        const double maxHarmonics = 0.5 * sampleRate / std::max (freq, 1.0);
        int m = 0;
        while (m < kMips - 1 && (double) (1024 >> m) > maxHarmonics)
            ++m;
        return m;
    }

private:
    WavetableBank();
    std::vector<float> data[kNumTables];
};
} // namespace icewave
