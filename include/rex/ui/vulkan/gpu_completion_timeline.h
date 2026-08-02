#pragma once
/**
 * @file        ui/vulkan/gpu_completion_timeline.h
 * @brief       Canary VulkanGPUCompletionTimeline surface over ReXGlue's
 *              VulkanSubmissionTracker.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 *
 * @remarks     Xenia Canary reworked the Vulkan submission timeline into
 *              `xe::ui::vulkan::VulkanGPUCompletionTimeline`, which owns the queue
 *              submit. ReXGlue keeps its `VulkanSubmissionTracker` (fence
 *              acquisition + submission-index bookkeeping); this thin adapter
 *              presents the Canary GPU command processor's expected surface over
 *              it. The submit path mirrors the preserved ReXGlue vulkan
 *              command_processor (AcquireQueue + functions().vkQueueSubmit + the
 *              FenceAcquisition failure contract).
 */

#ifndef REX_UI_VULKAN_GPU_COMPLETION_TIMELINE_H_
#define REX_UI_VULKAN_GPU_COMPLETION_TIMELINE_H_

#include <cstdint>

#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/submission_tracker.h>

namespace rex::ui::vulkan {

class VulkanGPUCompletionTimeline {
 public:
  explicit VulkanGPUCompletionTimeline(const VulkanDevice* vulkan_device)
      : vulkan_device_(vulkan_device), submission_tracker_(vulkan_device) {}

  VulkanGPUCompletionTimeline(const VulkanGPUCompletionTimeline&) = delete;
  VulkanGPUCompletionTimeline& operator=(const VulkanGPUCompletionTimeline&) = delete;

  // The submission index the next AcquireFenceAndSubmit will use.
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
    last_completed_submission_ =
        submission_tracker_.UpdateAndGetCompletedSubmission();
    return reached;
  }

  // Acquire a fence for the current submission and submit `submits` on the given
  // queue, advancing the timeline. Mirrors the preserved command_processor path.
  VkResult AcquireFenceAndSubmit(uint32_t queue_family_index,
                                 uint32_t queue_index, uint32_t submit_count,
                                 const VkSubmitInfo* submits) {
    auto fence_acquisition = submission_tracker_.AcquireFenceToAdvanceSubmission();
    auto queue_acquisition =
        vulkan_device_->AcquireQueue(queue_family_index, queue_index);
    VkResult result = vulkan_device_->functions().vkQueueSubmit(
        queue_acquisition.queue(), submit_count, submits,
        fence_acquisition.fence());
    if (result != VK_SUCCESS) {
      fence_acquisition.SubmissionFailedOrDropped();
    }
    return result;
  }

  void Shutdown() { submission_tracker_.Shutdown(); }

 private:
  const VulkanDevice* vulkan_device_ = nullptr;
  VulkanSubmissionTracker submission_tracker_;
  uint64_t last_completed_submission_ = 0;
};

}  // namespace rex::ui::vulkan

#endif  // REX_UI_VULKAN_GPU_COMPLETION_TIMELINE_H_
