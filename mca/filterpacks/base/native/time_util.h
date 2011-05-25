// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef VIDEO_FILTER_TIME_UTIL_H_
#define VIDEO_FILTER_TIME_UTIL_H_

#include <string>
#include <utils/RefBase.h>

#include "basictypes.h"

#define LOG_MFF_RUNNING_TIMES 0

namespace android {
namespace mff {

uint64_t getTimeUs();

class NamedStopWatch : public RefBase {
  public:
    static const uint64_t kDefaultLoggingPeriodInFrames;

    explicit NamedStopWatch(const string& name);
    void Start();
    void Stop();

    void SetName(const string& name) { mName = name; }
    void SetLoggingPeriodInFrames(uint64_t numFrames) {
        mLoggingPeriodInFrames = numFrames;
    }

    const string& Name() const { return mName; }
    uint64_t NumCalls() const { return mNumCalls; }
    uint64_t TotalUSec() const { return mTotalUSec; }

  private:
    string mName;
    uint64_t mLoggingPeriodInFrames;
    uint64_t mStartUSec;
    uint64_t mNumCalls;
    uint64_t mTotalUSec;
};

class ScopedTimer {
  public:
    explicit ScopedTimer(const string& stop_watch_name);
    explicit ScopedTimer(NamedStopWatch* watch)
        : mWatch(watch) { mWatch->Start(); }
    ~ScopedTimer() { mWatch->Stop(); }

  private:
    NamedStopWatch* mWatch;
};

} // namespace mff
} // namespace android

#endif  // VIDEO_FILTER_TIME_UTIL_H_
