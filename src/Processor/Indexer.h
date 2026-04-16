#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include "BeatmapHash.h"
#include "../Database/Database.h"

namespace Orr::Processor {

    struct Indexer {
        Indexer(const Indexer&) = delete;
        Indexer& operator=(const Indexer&) = delete;

        Indexer(Orr::Database::BeatmapDb& bdb, const std::string& dirPath, int threadCount) {
            mThread = std::thread([&, dirPath, threadCount]() {
                BeatmapHash::BuildIndex(bdb, dirPath, threadCount,
                    [&](const BeatmapHash::BuildIndexStatus& s) {
                        std::lock_guard lock(mMutex);
                        mStatus = s;
                    });
                mDone = true;
            });
        }

        ~Indexer() {
            if (mThread.joinable()) {
                mThread.join();
            }
        }

        bool IsDone() const {
            return mDone;
        }

        BeatmapHash::BuildIndexStatus GetStatus() {
            std::lock_guard lock(mMutex);
            return mStatus;
        }

    private:
        std::thread mThread;
        std::mutex mMutex;
        std::atomic<bool> mDone = false;
        BeatmapHash::BuildIndexStatus mStatus;
    };

}