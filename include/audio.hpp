#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

// Short two-note chime on the desktop's default sink. The samples are handed to paplay
// (PipeWire's PulseAudio shim serves it too), so there is no extra runtime dependency.
class Audio {
    static constexpr int    RATE      = 48000;   // native for PipeWire, so nothing resamples
    static constexpr int    ATTACK_MS = 4;       // short, but not a step: no click at the start
    static constexpr double PEAK      = 0.75;    // of full scale, roughly -4 dBFS

    // Sine plus a quiet octave: the octave adds presence on small speakers without the buzz a
    // square wave would bring. The envelope opens in a few milliseconds and decays
    // exponentially, which reads as a soft chime rather than a buzzer.
    static void note(std::vector<int16_t>& pcm, double freq, double ms) {
        const int n = std::max(1, (int)(RATE * ms / 1000.0));
        const int attack = std::max(1, RATE * ATTACK_MS / 1000);
        for (int i = 0; i < n; i++) {
            const double t = (double)i / RATE;
            const double wave = std::sin(2.0 * M_PI * freq * t)
                              + 0.25 * std::sin(4.0 * M_PI * freq * t);
            const double env = std::min(1.0, (double)i / attack) * std::exp(-5.0 * i / n);
            pcm.push_back((int16_t)(PEAK * env * wave / 1.25 * 32767.0));
        }
    }

    static void play(double f1, double ms1, double f2, double ms2) {
        std::thread([=] {
            std::vector<int16_t> pcm;
            pcm.reserve((size_t)(RATE * (ms1 + ms2) / 1000.0) + 2);
            note(pcm, f1, ms1);
            note(pcm, f2, ms2);
            FILE* pipe = popen("paplay --raw --format=s16le --rate=48000 --channels=1 2>/dev/null", "w");
            if (pipe) {
                fwrite(pcm.data(), sizeof(int16_t), pcm.size(), pipe);
                pclose(pipe);
            }
        }).detach();
    }

public:
    static void beep_on()  { play(784.0, 45, 1175.0, 60); }   // G5 -> D6, rising
    static void beep_off() { play(1175.0, 45, 784.0, 60); }   // D6 -> G5, falling
};
