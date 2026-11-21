#include "Wavetables.h"
#include <cmath>
#include <complex>

namespace icewave
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr int kMaxHarm = 1024;

void inverseFFT (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * pi / len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int j = 0; j < len / 2; ++j)
            {
                auto u = a[(size_t) (i + j)];
                auto v = a[(size_t) (i + j + len / 2)] * w;
                a[(size_t) (i + j)] = u + v;
                a[(size_t) (i + j + len / 2)] = u - v;
                w *= wl;
            }
        }
    }
}

// sinA[n] and cosB[n], n = 1..kMaxHarm
void basicShapes (double t, std::vector<double>& s, std::vector<double>& c)
{
    auto shape = [] (int which, int n) -> double
    {
        switch (which)
        {
            case 0: return n == 1 ? 1.0 : 0.0;                                         // sine
            case 1: return (n % 2) ? (8.0 / (pi * pi)) * (((n - 1) / 2) % 2 ? -1.0 : 1.0) / (double) (n * n) : 0.0; // triangle
            case 2: return (2.0 / pi) * ((n % 2) ? 1.0 : -1.0) / n;                    // saw
            default: return (n % 2) ? 4.0 / (pi * n) : 0.0;                            // square
        }
    };
    const double seg = t * 3.0;
    const int i0 = std::min (2, (int) seg);
    const double fr = seg - i0;
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        s[(size_t) n] = shape (i0, n) * (1.0 - fr) + shape (i0 + 1, n) * fr;
        c[(size_t) n] = 0.0;
    }
}

void pwm (double t, std::vector<double>& s, std::vector<double>& c)
{
    const double w = 0.5 - 0.46 * t;
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        s[(size_t) n] = 0.0;
        c[(size_t) n] = (2.0 / (n * pi)) * std::sin (n * pi * w);
    }
}

void harmonicSweep (double t, std::vector<double>& s, std::vector<double>& c)
{
    const double centre = 1.0 + t * 40.0;
    const double width  = 1.5 + t * 6.0;
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const double d = (n - centre) / width;
        double a = std::exp (-0.5 * d * d);
        if (n == 1) a += 0.6;
        s[(size_t) n] = a / std::sqrt ((double) n);
        c[(size_t) n] = 0.0;
    }
}

void vocal (double t, std::vector<double>& s, std::vector<double>& c)
{
    static const double formants[5][3] = {
        { 730, 1090, 2440 }, // A
        { 530, 1840, 2480 }, // E
        { 270, 2290, 3010 }, // I
        { 570,  840, 2410 }, // O
        { 300,  870, 2240 }  // U
    };
    const double bw[3]   = { 90, 120, 170 };
    const double gain[3] = { 1.0, 0.6, 0.35 };
    const double f0 = 130.0;

    const double seg = t * 4.0;
    const int i0 = std::min (3, (int) seg);
    const double fr = seg - i0;
    double F[3];
    for (int k = 0; k < 3; ++k)
        F[k] = formants[i0][k] * (1.0 - fr) + formants[i0 + 1][k] * fr;

    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const double f = n * f0;
        double env = 0.02;
        for (int k = 0; k < 3; ++k)
        {
            const double d = (f - F[k]) / bw[k];
            env += gain[k] * std::exp (-0.5 * d * d);
        }
        s[(size_t) n] = env / std::pow ((double) n, 0.4) * ((n % 2) ? 1.0 : -1.0);
        c[(size_t) n] = 0.0;
    }
}
} // namespace

const char* WavetableBank::tableName (int i)
{
    static const char* names[] = { "Basic Shapes", "PWM", "Harmonic Sweep", "Vocal" };
    return names[std::clamp (i, 0, kNumTables - 1)];
}

WavetableBank::WavetableBank()
{
    std::vector<double> s (kMaxHarm + 1), c (kMaxHarm + 1);
    std::vector<std::complex<double>> buf (kTableSize);

    for (int table = 0; table < kNumTables; ++table)
    {
        data[table].assign ((size_t) kFrames * kMips * (kTableSize + 1), 0.0f);

        for (int f = 0; f < kFrames; ++f)
        {
            const double t = (double) f / (kFrames - 1);
            switch (table)
            {
                case 0: basicShapes (t, s, c); break;
                case 1: pwm (t, s, c); break;
                case 2: harmonicSweep (t, s, c); break;
                default: vocal (t, s, c); break;
            }

            double scale = 1.0;
            for (int m = 0; m < kMips; ++m)
            {
                const int maxH = 1024 >> m;
                std::fill (buf.begin(), buf.end(), std::complex<double> (0.0, 0.0));
                for (int n = 1; n <= maxH; ++n)
                    buf[(size_t) n] = std::complex<double> (c[(size_t) n], -s[(size_t) n]);
                inverseFFT (buf);

                if (m == 0)
                {
                    double peak = 1.0e-9;
                    for (auto& v : buf) peak = std::max (peak, std::abs (v.real()));
                    scale = 1.0 / peak;
                }

                float* dst = data[table].data() + ((size_t) f * kMips + (size_t) m) * (kTableSize + 1);
                for (int i = 0; i < kTableSize; ++i)
                    dst[i] = (float) (buf[(size_t) i].real() * scale);
                dst[kTableSize] = dst[0];
            }
        }
    }
}
} // namespace icewave
