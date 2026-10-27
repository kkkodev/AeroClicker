#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>

// Toggle feedback: a two-note chime rendered in software and piped to paplay.
//
// Each note is a sine plus a quiet octave harmonic, which gives it a bit of body instead of
// the flat, buzzy edge of a bare sine, and it is enveloped (near-instant attack, exponential
// decay to silence) so the note dies away rather than being cut off mid-cycle — a hard cut is
// what makes a tone click. The two notes form a perfect fourth, rising to switch on and
// falling to switch off, which reads as a chime rather than an alarm.
class Audio {
    static constexpr int RATE = 44100;
    // Peak of the note, as a fraction of full scale. paplay has no gain stage of its own, so
    // this is the only level control; the old tones peaked around 15%, which is why they were
    // barely audible next to desktop sounds.
    static constexpr double PEAK = 0.70;
    // Peak of sin(x) + 0.25*sin(2x): normalising by it keeps PEAK a true fraction of full
    // scale, so the louder harmonic cannot push the samples into clipping.
    static constexpr double BODY_PEAK = 1.11;
    static constexpr double HARMONIC = 0.25;

    static void play_tone(double freq1, double dur1_ms, double freq2, double dur2_ms) {
        std::thread([=]() {
            const int s1 = (int)(RATE * dur1_ms / 1000.0);
            const int s2 = (int)(RATE * dur2_ms / 1000.0);
            const int total = s1 + s2;
            if (total <= 0) return;
            auto* buf = new int16_t[total];
            int pos = 0;

            auto gen = [&](double freq, int samples) {
                const double attack = 0.002 * RATE;   // 2 ms: instant to the ear, no click
                const double last = samples > 1 ? (double)(samples - 1) : 1.0;
                for (int i = 0; i < samples && pos < total; i++, pos++) {
                    const double t = (double)i / RATE;
                    double env = i < attack ? (double)i / attack : 1.0;
                    env *= exp(-6.9 * (double)i / last);   // fades to about -60 dB by the end
                    const double body = (std::sin(2.0 * M_PI * freq * t)
                                       + HARMONIC * std::sin(4.0 * M_PI * freq * t)) / BODY_PEAK;
                    buf[pos] = (int16_t)(PEAK * env * 32767.0 * body);
                }
            };

            gen(freq1, s1);
            gen(freq2, s2);

            FILE* pipe = popen("paplay --raw --format=s16le --rate=44100 --channels=1 2>/dev/null", "w");
            if (pipe) {
                // If paplay went away mid-beep (no audio server, killed) the write fails or the
                // pipe breaks; SIGPIPE is ignored process-wide, so this is only a short write
                // and there is nothing left to salvage.
                const size_t want = (size_t)total;
                const size_t wrote = fwrite(buf, sizeof(int16_t), want, pipe);
                (void)wrote;
                pclose(pipe);
            }
            delete[] buf;
        }).detach();
    }

public:
    // G5 -> C6 (a fourth up) for on, its mirror for off.
    static void beep_on()  { play_tone(784, 55, 1046, 110); }
    static void beep_off() { play_tone(1046, 55, 784, 110); }
};
