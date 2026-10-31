#pragma once
#include <cstdint>
#include <optional>
#include <cmath>

namespace DanceStudio {
enum class SessionState { Idle, Preparing, Armed, Playing, Stopping };
enum class ReadyPart : std::uint32_t {
    PlayerGraph = 1, CameraActor3D = 2, CameraTrack = 4, AudioBuffered = 8, WorldValid = 16
};
// Engine-independent session policy, used by the native SKSE adapter.
// Readiness must be reported by engine completion callbacks, not file checks.
class SessionCore {
public:
    static constexpr std::uint32_t AllReady = 31;
    std::optional<std::uint64_t> Prepare(double now, double timeout = 30) {
        if (state_ != SessionState::Idle || !std::isfinite(now) || !std::isfinite(timeout) || timeout <= 0) return std::nullopt;
        ++generation_; if (!generation_) ++generation_;
        state_ = SessionState::Preparing;
        startRequested_=false; ready_ = 0; frames_ = 0; rate_ = 0; seconds_ = 0; failed_ = false;
        timeout_ = timeout; deadline_ = now + timeout;
        return generation_;
    }
    bool Ready(std::uint64_t generation, ReadyPart part, bool value, double now) {
        if (generation != generation_ || !std::isfinite(now)) return false;
        const auto bit = static_cast<std::uint32_t>(part);
        if (!bit || (bit & ~AllReady) || (bit & (bit - 1))) return false;
        if (state_ == SessionState::Playing && !value) return Stop(true);
        if (state_ != SessionState::Preparing && state_ != SessionState::Armed) return false;
        if (value) ready_ |= bit; else ready_ &= ~bit;
        if (ready_ == AllReady) state_ = SessionState::Armed;
        else {
            if (state_ == SessionState::Armed) deadline_ = now + timeout_;
            state_ = SessionState::Preparing;
        }
        return true;
    }
    // Start requests wait for readiness; the adapter also checks menus/world before Start.
    bool RequestStart(){if(state_!=SessionState::Preparing&&state_!=SessionState::Armed)return false;startRequested_=true;return true;}
    bool PendingStart()const{return startRequested_;}
    bool Start() {
        if (state_ != SessionState::Armed || ready_ != AllReady) return false;
        startRequested_=false; state_ = SessionState::Playing;
        return true;
    }
    bool AudioProgress(std::uint64_t generation, std::uint64_t frames, std::uint32_t sampleRate) {
        if (generation != generation_ || state_ != SessionState::Playing) return false;
        if (!sampleRate || frames < frames_ || (rate_ && rate_ != sampleRate)) { Stop(true); return false; }
        rate_ = sampleRate;
        frames_ = frames; seconds_ = static_cast<double>(frames) / sampleRate;
        return true;
    }
    bool Tick(double now) {
        if (state_ == SessionState::Preparing && now >= deadline_) return Stop(true);
        return false;
    }
    // The SKSE adapter restores camera/controls/audio/actor state exactly once,
    // then acknowledges cleanup on the game thread. Old callbacks are inert.
    bool Stop(bool failed = false) {
        if (state_ == SessionState::Idle || state_ == SessionState::Stopping) return false;
        startRequested_=false; state_ = SessionState::Stopping; failed_ = failed;
        ++generation_; return true;
    }
    bool Cleaned() {
        if (state_ != SessionState::Stopping) return false;
        state_ = SessionState::Idle; ready_ = 0; return true;
    }
    SessionState State() const { return state_; }
    double Seconds() const { return seconds_; }
    bool Failed() const { return failed_; }
    std::uint64_t Generation() const { return generation_; }
private:
    SessionState state_{SessionState::Idle};
    std::uint64_t generation_{0}, frames_{0};
    std::uint32_t ready_{0}, rate_{0};
    double deadline_{0}, timeout_{30}, seconds_{0};
    bool failed_{false},startRequested_{};
};
}
