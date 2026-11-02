#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>

struct ClickData {
    int64_t timestamp_ms;
    double target_cps;
    double actual_cps;
    int64_t hold_ns;
    int64_t interval_ns;
    double trend;
};

class Tracker {
    static constexpr int WINDOW = 20;
    static constexpr int MAX_HISTORY = 2000;
    static constexpr int GRAPH_SIZE = 120;

    // The clicker thread records into every member below while the UI thread reads them for
    // the readouts and the graph, so all of it sits behind mu_. The old version locked only
    // the CSV history, which left the averages and the graph ring as unchecked races.
    mutable std::mutex mu_;

    double cps_samples_[WINDOW] = {};
    double hold_samples_[WINDOW] = {};
    int pos_ = 0;
    int count_ = 0;
    uint64_t total_clicks_ = 0;

    float cps_graph_[GRAPH_SIZE] = {};
    int graph_pos_ = 0;
    int graph_count_ = 0;

    std::deque<ClickData> history_;
    std::atomic<bool> recording_{false};

    static int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

public:
    // target_cps is what the humanization model asked for; actual_cps is measured from the
    // real press-to-press time, so the readout and the CSV show what actually happened.
    void record(double target_cps, double actual_cps, double hold_ns, double interval_ns,
                double trend) {
        std::lock_guard<std::mutex> lock(mu_);
        cps_samples_[pos_] = actual_cps;
        hold_samples_[pos_] = hold_ns / 1'000'000.0;
        pos_ = (pos_ + 1) % WINDOW;
        if (count_ < WINDOW) count_++;
        total_clicks_++;

        cps_graph_[graph_pos_] = static_cast<float>(actual_cps);
        graph_pos_ = (graph_pos_ + 1) % GRAPH_SIZE;
        if (graph_count_ < GRAPH_SIZE) graph_count_++;

        if (recording_) {
            if (history_.size() >= MAX_HISTORY)
                history_.pop_front();
            history_.push_back({now_ms(), target_cps, actual_cps, static_cast<int64_t>(hold_ns),
                                static_cast<int64_t>(interval_ns), trend});
        }
    }

    double avg_cps() const {
        std::lock_guard<std::mutex> lock(mu_);
        if (count_ == 0) return 0;
        double sum = 0;
        for (int i = 0; i < count_; i++) sum += cps_samples_[i];
        return sum / count_;
    }

    double avg_hold_ms() const {
        std::lock_guard<std::mutex> lock(mu_);
        if (count_ == 0) return 0;
        double sum = 0;
        for (int i = 0; i < count_; i++) sum += hold_samples_[i];
        return sum / count_;
    }

    uint64_t total() const {
        std::lock_guard<std::mutex> lock(mu_);
        return total_clicks_;
    }

    // Copies the ring so the caller can draw it without holding the lock (returning a pointer
    // to the live buffer, as the old accessor did, made the draw another race).
    void graph_copy(float* out, int& offset) const {
        std::lock_guard<std::mutex> lock(mu_);
        for (int i = 0; i < GRAPH_SIZE; i++) out[i] = cps_graph_[i];
        offset = graph_pos_;
    }

    static constexpr int graph_size() { return GRAPH_SIZE; }

    void set_recording(bool v) { recording_ = v; }
    bool is_recording() const { return recording_; }

    int history_size() const {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int>(history_.size());
    }

    void clear_history() {
        std::lock_guard<std::mutex> lock(mu_);
        history_.clear();
    }

    bool export_csv(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(mu_);
        FILE* f = fopen(filepath.c_str(), "w");
        if (!f) return false;

        fprintf(f, "Timestamp,TargetCPS,ActualCPS,HoldMS,IntervalMS,Trend\n");
        for (const auto& d : history_) {
            fprintf(f, "%ld,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    (long)d.timestamp_ms, d.target_cps, d.actual_cps,
                    d.hold_ns / 1'000'000.0, d.interval_ns / 1'000'000.0, d.trend);
        }
        fclose(f);
        return true;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mu_);
        pos_ = 0;
        count_ = 0;
        total_clicks_ = 0;
        graph_pos_ = 0;
        graph_count_ = 0;
        for (float& v : cps_graph_) v = 0.0f;
        history_.clear();
    }
};
