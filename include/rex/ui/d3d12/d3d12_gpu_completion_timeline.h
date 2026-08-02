#pragma once
/**
 * @file        ui/d3d12/d3d12_gpu_completion_timeline.h
 * @brief       Canary D3D12GPUCompletionTimeline surface over ReXGlue's
 *              D3D12SubmissionTracker.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 *
 * @remarks     Xenia Canary reworked the D3D12 submission timeline into
 *              `xe::ui::d3d12::D3D12GPUCompletionTimeline` (created via a factory,
 *              signalling on an explicitly-passed queue and returning HRESULT).
 *              ReXGlue keeps its `D3D12SubmissionTracker`; this adapter presents
 *              the Canary GPU command processor's expected surface over it:
 *              SignalAndAdvance -> SetQueue + NextSubmission, the await/getter pair
 *              -> AwaitSubmissionCompletion + GetCompletedSubmission.
 *
 *              NOTE: the D3D12 backend is not built on macOS; this Windows-only
 *              path (and its fence/signal semantics) still needs a Windows-build
 *              verification pass, though the API mapping to D3D12SubmissionTracker
 *              is direct.
 */

#ifndef REX_UI_D3D12_D3D12_GPU_COMPLETION_TIMELINE_H_
#define REX_UI_D3D12_D3D12_GPU_COMPLETION_TIMELINE_H_

#include <cstdint>
#include <memory>

#include <rex/ui/d3d12/d3d12_submission_tracker.h>

namespace rex::ui::d3d12 {

class D3D12GPUCompletionTimeline {
 public:
  static std::unique_ptr<D3D12GPUCompletionTimeline> Create(ID3D12Device* device) {
    std::unique_ptr<D3D12GPUCompletionTimeline> timeline(
        new D3D12GPUCompletionTimeline());
    // The queue is bound dynamically per SignalAndAdvance, so init with none.
    if (!timeline->submission_tracker_.Initialize(device, nullptr)) {
      return nullptr;
    }
    return timeline;
  }

  D3D12GPUCompletionTimeline(const D3D12GPUCompletionTimeline&) = delete;
  D3D12GPUCompletionTimeline& operator=(const D3D12GPUCompletionTimeline&) = delete;

  // The submission index the next SignalAndAdvance will use.
  uint64_t GetUpcomingSubmission() const {
    return submission_tracker_.GetCurrentSubmission();
  }

  // The completed-submission value cached at the last update (no re-query).
  uint64_t GetCompletedSubmissionFromLastUpdate() const {
    return last_completed_submission_;
  }

  // Await the given submission, then refresh the cached completed value. Returns
  // whether the GPU signal was actually reached (vs a fallback condition).
  bool AwaitSubmissionAndUpdateCompleted(uint64_t submission_index) {
    bool reached = submission_tracker_.AwaitSubmissionCompletion(submission_index);
    last_completed_submission_ = submission_tracker_.GetCompletedSubmission();
    return reached;
  }

  // Enqueue a fence signal on `queue` and advance the timeline.
  HRESULT SignalAndAdvance(ID3D12CommandQueue* queue) {
    submission_tracker_.SetQueue(queue);
    return submission_tracker_.NextSubmission() ? S_OK : E_FAIL;
  }

  bool AwaitAllSubmissions() {
    return submission_tracker_.AwaitAllSubmissionsCompletion();
  }

  void Shutdown() { submission_tracker_.Shutdown(); }

 private:
  D3D12GPUCompletionTimeline() = default;

  D3D12SubmissionTracker submission_tracker_;
  uint64_t last_completed_submission_ = 0;
};

}  // namespace rex::ui::d3d12

#endif  // REX_UI_D3D12_D3D12_GPU_COMPLETION_TIMELINE_H_
