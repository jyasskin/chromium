// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cc/scheduler/scheduler.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_ACTION(action, client, action_index, expected_num_actions) \
  EXPECT_EQ(expected_num_actions, client.num_actions_());                 \
  ASSERT_LT(action_index, client.num_actions_());                         \
  do {                                                                    \
    EXPECT_STREQ(action, client.Action(action_index));                    \
    for (int i = expected_num_actions; i < client.num_actions_(); ++i)    \
      ADD_FAILURE() << "Unexpected action: " << client.Action(i) <<       \
          " with state:\n" << client.StateForAction(action_index);        \
  } while (false)

#define EXPECT_SINGLE_ACTION(action, client) \
  EXPECT_ACTION(action, client, 0, 1)

namespace cc {
namespace {

class FakeSchedulerClient;

void InitializeOutputSurfaceAndFirstCommit(Scheduler* scheduler,
                                           FakeSchedulerClient* client);

class FakeSchedulerClient : public SchedulerClient {
 public:
  FakeSchedulerClient() : needs_begin_frame_(false), automatic_swap_ack_(true) {
    Reset();
  }

  void Reset() {
    actions_.clear();
    states_.clear();
    draw_will_happen_ = true;
    swap_will_happen_if_draw_happens_ = true;
    num_draws_ = 0;
    log_anticipated_draw_time_change_ = false;
  }

  Scheduler* CreateScheduler(const SchedulerSettings& settings) {
    task_runner_ = new base::TestSimpleTaskRunner;
    scheduler_ = Scheduler::Create(this, settings, 0, task_runner_);
    return scheduler_.get();
  }

  // Most tests don't care about DidAnticipatedDrawTimeChange, so only record it
  // for tests that do.
  void set_log_anticipated_draw_time_change(bool log) {
    log_anticipated_draw_time_change_ = log;
  }
  bool needs_begin_frame() { return needs_begin_frame_; }
  int num_draws() const { return num_draws_; }
  int num_actions_() const { return static_cast<int>(actions_.size()); }
  const char* Action(int i) const { return actions_[i]; }
  base::Value& StateForAction(int i) const { return *states_[i]; }
  base::TimeTicks posted_begin_impl_frame_deadline() const {
    return posted_begin_impl_frame_deadline_;
  }

  base::TestSimpleTaskRunner& task_runner() { return *task_runner_; }

  int ActionIndex(const char* action) const {
    for (size_t i = 0; i < actions_.size(); i++)
      if (!strcmp(actions_[i], action))
        return i;
    return -1;
  }

  bool HasAction(const char* action) const {
    return ActionIndex(action) >= 0;
  }

  void SetDrawWillHappen(bool draw_will_happen) {
    draw_will_happen_ = draw_will_happen;
  }
  void SetSwapWillHappenIfDrawHappens(bool swap_will_happen_if_draw_happens) {
    swap_will_happen_if_draw_happens_ = swap_will_happen_if_draw_happens;
  }
  void SetAutomaticSwapAck(bool automatic_swap_ack) {
    automatic_swap_ack_ = automatic_swap_ack;
  }

  // SchedulerClient implementation.
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE {
    actions_.push_back("SetNeedsBeginFrame");
    states_.push_back(scheduler_->StateAsValue().release());
    needs_begin_frame_ = enable;
  }
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) OVERRIDE {
    actions_.push_back("WillBeginImplFrame");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {
    actions_.push_back("ScheduledActionSendBeginMainFrame");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionAnimate() OVERRIDE {
    actions_.push_back("ScheduledActionAnimate");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapIfPossible");
    states_.push_back(scheduler_->StateAsValue().release());
    num_draws_++;
    bool did_readback = false;
    DrawSwapReadbackResult::DrawResult result =
        draw_will_happen_
            ? DrawSwapReadbackResult::DRAW_SUCCESS
            : DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
    bool swap_will_happen =
        draw_will_happen_ && swap_will_happen_if_draw_happens_;
    if (swap_will_happen) {
      scheduler_->DidSwapBuffers();
      if (automatic_swap_ack_)
        scheduler_->DidSwapBuffersComplete();
    }
    return DrawSwapReadbackResult(
        result,
        draw_will_happen_ && swap_will_happen_if_draw_happens_,
        did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapForced");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_request_swap = swap_will_happen_if_draw_happens_;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_request_swap, did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndReadback() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndReadback");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_request_swap = false;
    bool did_readback = true;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_request_swap, did_readback);
  }
  virtual void ScheduledActionCommit() OVERRIDE {
    actions_.push_back("ScheduledActionCommit");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE {
    actions_.push_back("ScheduledActionUpdateVisibleTiles");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionActivatePendingTree() OVERRIDE {
    actions_.push_back("ScheduledActionActivatePendingTree");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {
    actions_.push_back("ScheduledActionBeginOutputSurfaceCreation");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionManageTiles() OVERRIDE {
    actions_.push_back("ScheduledActionManageTiles");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {
    if (log_anticipated_draw_time_change_)
      actions_.push_back("DidAnticipatedDrawTimeChange");
  }
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }

  virtual void DidBeginImplFrameDeadline() OVERRIDE {}

 protected:
  bool needs_begin_frame_;
  bool draw_will_happen_;
  bool swap_will_happen_if_draw_happens_;
  bool automatic_swap_ack_;
  int num_draws_;
  bool log_anticipated_draw_time_change_;
  base::TimeTicks posted_begin_impl_frame_deadline_;
  std::vector<const char*> actions_;
  ScopedVector<base::Value> states_;
  scoped_ptr<Scheduler> scheduler_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

void InitializeOutputSurfaceAndFirstCommit(Scheduler* scheduler,
                                           FakeSchedulerClient* client) {
  bool client_initiates_begin_frame =
      scheduler->settings().begin_frame_scheduling_enabled &&
      scheduler->settings().throttle_frame_production;

  scheduler->DidCreateAndInitializeOutputSurface();
  scheduler->SetNeedsCommit();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  // Go through the motions to draw the commit.
  if (client_initiates_begin_frame)
    scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  else
    client->task_runner().RunPendingTasks();  // Run posted BeginFrame.

  // Run the posted deadline task.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client->task_runner().RunPendingTasks();
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // We need another BeginImplFrame so Scheduler calls
  // SetNeedsBeginFrame(false).
  if (client_initiates_begin_frame)
    scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  else
    client->task_runner().RunPendingTasks();  // Run posted BeginFrame.

  // Run the posted deadline task.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client->task_runner().RunPendingTasks();
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
}

TEST(SchedulerTest, InitializeOutputSurfaceDoesNotBeginImplFrame) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();
  EXPECT_EQ(0, client.num_actions_());
}

TEST(SchedulerTest, RequestCommit) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  client.Reset();

  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // If we don't swap on the deadline, we wait for the next BeginFrame.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame should prepare the draw.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 1);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, RequestCommitAfterBeginMainFrameSent) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  // SetNeedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);

  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Now SetNeedsCommit again. Calling here means we need a second commit.
  scheduler->SetNeedsCommit();
  EXPECT_EQ(client.num_actions_(), 0);
  client.Reset();

  // Finish the first commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Because we just swapped, the Scheduler should also request the next
  // BeginImplFrame from the OutputSurface.
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();
  // Since another commit is needed, the next BeginImplFrame should initiate
  // the second commit.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  // Finishing the commit before the deadline should post a new deadline task
  // to trigger the deadline early.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // On the next BeginImplFrame, verify we go back to a quiescent state and
  // no longer request BeginImplFrames.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

class SchedulerClientThatsetNeedsDrawInsideDraw : public FakeSchedulerClient {
 public:
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {}
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    // Only SetNeedsRedraw the first time this is called
    if (!num_draws_)
      scheduler_->SetNeedsRedraw();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    NOTREACHED();
    bool did_request_swap = true;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_request_swap, did_readback);
  }

  virtual void ScheduledActionCommit() OVERRIDE {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {}
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}
};

// Tests for two different situations:
// 1. the scheduler dropping SetNeedsRedraw requests that happen inside
//    a ScheduledActionDrawAndSwap
// 2. the scheduler drawing twice inside a single tick
TEST(SchedulerTest, RequestRedrawInsideDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_frame());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(SchedulerTest, RequestRedrawInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeSchedulerClient {
 public:
  SchedulerClientThatSetNeedsCommitInsideDraw()
      : set_needs_commit_on_next_draw_(false) {}

  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {}
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    // Only SetNeedsCommit the first time this is called
    if (set_needs_commit_on_next_draw_) {
      scheduler_->SetNeedsCommit();
      set_needs_commit_on_next_draw_ = false;
    }
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    NOTREACHED();
    bool did_request_swap = false;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_request_swap, did_readback);
  }

  virtual void ScheduledActionCommit() OVERRIDE {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {}
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}

  void SetNeedsCommitOnNextDraw() { set_needs_commit_on_next_draw_ = true; }

 private:
  bool set_needs_commit_on_next_draw_;
};

// Tests for the scheduler infinite-looping on SetNeedsCommit requests that
// happen inside a ScheduledActionDrawAndSwap
TEST(SchedulerTest, RequestCommitInsideDraw) {
  SchedulerClientThatSetNeedsCommitInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  EXPECT_FALSE(client.needs_begin_frame());
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_TRUE(client.needs_begin_frame());

  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.SetNeedsCommitOnNextDraw();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_frame());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();

  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());

  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_FALSE(client.needs_begin_frame());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(SchedulerTest, RequestCommitInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());

  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
}

TEST(SchedulerTest, NoSwapWhenDrawFails) {
  SchedulerClientThatSetNeedsCommitInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Draw successfully, this starts a new frame.
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail to draw, this should not start a frame.
  client.SetDrawWillHappen(false);
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client.num_draws());
}

TEST(SchedulerTest, NoSwapWhenSwapFailsDuringForcedCommit) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  // Tell the client that it will fail to swap.
  client.SetDrawWillHappen(true);
  client.SetSwapWillHappenIfDrawHappens(false);

  // Get the compositor to do a ScheduledActionDrawAndReadback.
  scheduler->SetCanDraw(true);
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));
}

TEST(SchedulerTest, BackToBackReadbackAllowed) {
  // Some clients call readbacks twice in a row before the replacement
  // commit comes in.  Make sure it is allowed.
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  // Get the compositor to do 2 ScheduledActionDrawAndReadbacks before
  // the replacement commit comes in.
  scheduler->SetCanDraw(true);
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  client.Reset();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  // The replacement commit comes in after 2 readbacks.
  client.Reset();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
}


class SchedulerClientNeedsManageTilesInDraw : public FakeSchedulerClient {
 public:
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    scheduler_->SetNeedsManageTiles();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }
};

// Test manage tiles is independant of draws.
TEST(SchedulerTest, ManageTiles) {
  SchedulerClientNeedsManageTilesInDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // Request both draw and manage tiles. ManageTiles shouldn't
  // be trigged until BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  // On the deadline, he actions should have occured in the right order.
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Request a draw. We don't need a ManageTiles yet.
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  // Draw. The draw will trigger SetNeedsManageTiles, and
  // then the ManageTiles action will be triggered after the Draw.
  // Afterwards, neither a draw nor ManageTiles are pending.
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // We need a BeginImplFrame where we don't swap to go idle.
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  EXPECT_FALSE(client.needs_begin_frame());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_EQ(0, client.num_draws());

  // Now trigger a ManageTiles outside of a draw. We will then need
  // a begin-frame for the ManageTiles, but we don't need a draw.
  client.Reset();
  EXPECT_FALSE(client.needs_begin_frame());
  scheduler->SetNeedsManageTiles();
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->RedrawPending());

  // BeginImplFrame. There will be no draw, only ManageTiles.
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
}

// Test that ManageTiles only happens once per frame.  If an external caller
// initiates it, then the state machine should not ManageTiles on that frame.
TEST(SchedulerTest, ManageTilesOncePerFrame) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // If DidManageTiles during a frame, then ManageTiles should not occur again.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  EXPECT_FALSE(scheduler->ManageTilesPending());

  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Next frame without DidManageTiles should ManageTiles with draw.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  scheduler->DidManageTiles();  // Corresponds to ScheduledActionManageTiles

  // If we get another DidManageTiles within the same frame, we should
  // not ManageTiles on the next frame.
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());

  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // If we get another DidManageTiles, we should not ManageTiles on the next
  // frame. This verifies we don't alternate calling ManageTiles once and twice.
  EXPECT_TRUE(scheduler->ManageTilesPending());
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  EXPECT_FALSE(scheduler->ManageTilesPending());
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());

  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Next frame without DidManageTiles should ManageTiles with draw.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  scheduler->DidManageTiles();  // Corresponds to ScheduledActionManageTiles
}

TEST(SchedulerTest, TriggerBeginFrameDeadlineEarly) {
  SchedulerClientNeedsManageTilesInDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  client.Reset();
  scheduler->SetNeedsRedraw();
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());

  // The deadline should be zero since there is no work other than drawing
  // pending.
  EXPECT_EQ(base::TimeTicks(), client.posted_begin_impl_frame_deadline());
}

class SchedulerClientWithFixedEstimates : public FakeSchedulerClient {
 public:
  SchedulerClientWithFixedEstimates(
      base::TimeDelta draw_duration,
      base::TimeDelta begin_main_frame_to_commit_duration,
      base::TimeDelta commit_to_activate_duration)
      : draw_duration_(draw_duration),
        begin_main_frame_to_commit_duration_(
            begin_main_frame_to_commit_duration),
        commit_to_activate_duration_(commit_to_activate_duration) {}

  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE {
    return draw_duration_;
  }
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE {
    return begin_main_frame_to_commit_duration_;
  }
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE {
    return commit_to_activate_duration_;
  }

 private:
    base::TimeDelta draw_duration_;
    base::TimeDelta begin_main_frame_to_commit_duration_;
    base::TimeDelta commit_to_activate_duration_;
};

void MainFrameInHighLatencyMode(int64 begin_main_frame_to_commit_estimate_in_ms,
                                int64 commit_to_activate_estimate_in_ms,
                                bool smoothness_takes_priority,
                                bool should_send_begin_main_frame) {
  // Set up client with specified estimates (draw duration is set to 1).
  SchedulerClientWithFixedEstimates client(
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(
          begin_main_frame_to_commit_estimate_in_ms),
      base::TimeDelta::FromMilliseconds(commit_to_activate_estimate_in_ms));
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->SetSmoothnessTakesPriority(smoothness_takes_priority);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // Impl thread hits deadline before commit finishes.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_FALSE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_FALSE(scheduler->MainThreadIsInHighLatencyMode());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginMainFrame"));

  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->BeginFrame(CreateBeginFrameArgsForTesting());
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(scheduler->MainThreadIsInHighLatencyMode(),
            should_send_begin_main_frame);
  EXPECT_EQ(client.HasAction("ScheduledActionSendBeginMainFrame"),
            should_send_begin_main_frame);
}

TEST(SchedulerTest,
    SkipMainFrameIfHighLatencyAndCanCommitAndActivateBeforeDeadline) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 1, false, false);
}

TEST(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanCommitTooLong) {
  // Set up client so that estimates indicate that the commit cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(10, 1, false, true);
}

TEST(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanActivateTooLong) {
  // Set up client so that estimates indicate that the activate cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 10, false, true);
}

TEST(SchedulerTest, NotSkipMainFrameInPreferSmoothnessMode) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default), but also enable smoothness takes
  // priority mode.
  MainFrameInHighLatencyMode(1, 1, true, true);
}

TEST(SchedulerTest, PollForCommitCompletion) {
  // Since we are simulating a long commit, set up a client with draw duration
  // estimates that prevent skipping main frames to get to low latency mode.
  SchedulerClientWithFixedEstimates client(
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(32),
      base::TimeDelta::FromMilliseconds(32));
  client.set_log_anticipated_draw_time_change(true);
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  scheduler->SetCanDraw(true);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  EXPECT_TRUE(scheduler->CommitPending());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  scheduler->SetNeedsRedraw();

  BeginFrameArgs frame_args = CreateBeginFrameArgsForTesting();
  frame_args.interval = base::TimeDelta::FromMilliseconds(1000);
  scheduler->BeginFrame(frame_args);

  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  scheduler->DidSwapBuffers();
  scheduler->DidSwapBuffersComplete();

  // At this point, we've drawn a frame. Start another commit, but hold off on
  // the NotifyReadyToCommit for now.
  EXPECT_FALSE(scheduler->CommitPending());
  scheduler->SetNeedsCommit();
  scheduler->BeginFrame(frame_args);
  EXPECT_TRUE(scheduler->CommitPending());

  // Draw and swap the frame, but don't ack the swap to simulate the Browser
  // blocking on the renderer.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  scheduler->DidSwapBuffers();

  // Spin the event loop a few times and make sure we get more
  // DidAnticipateDrawTimeChange calls every time.
  int actions_so_far = client.num_actions_();

  // Does three iterations to make sure that the timer is properly repeating.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((frame_args.interval * 2).InMicroseconds(),
              client.task_runner().NextPendingTaskDelay().InMicroseconds())
        << *scheduler->StateAsValue();
    client.task_runner().RunPendingTasks();
    EXPECT_GT(client.num_actions_(), actions_so_far);
    EXPECT_STREQ(client.Action(client.num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client.num_actions_();
  }

  // Do the same thing after BeginMainFrame starts but still before activation.
  scheduler->NotifyBeginMainFrameStarted();
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((frame_args.interval * 2).InMicroseconds(),
              client.task_runner().NextPendingTaskDelay().InMicroseconds())
        << *scheduler->StateAsValue();
    client.task_runner().RunPendingTasks();
    EXPECT_GT(client.num_actions_(), actions_so_far);
    EXPECT_STREQ(client.Action(client.num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client.num_actions_();
  }
}

TEST(SchedulerTest, BeginRetroFrame) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  client.Reset();

  // Create a BeginFrame with a long deadline to avoid race conditions.
  // This is the first BeginFrame, which will be handled immediately.
  BeginFrameArgs args = CreateBeginFrameArgsForTesting();
  args.deadline += base::TimeDelta::FromHours(1);
  scheduler->BeginFrame(args);
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Queue BeginFrames while we are still handling the previous BeginFrame.
  args.frame_time += base::TimeDelta::FromSeconds(1);
  scheduler->BeginFrame(args);
  args.frame_time += base::TimeDelta::FromSeconds(1);
  scheduler->BeginFrame(args);

  // If we don't swap on the deadline, we wait for the next BeginImplFrame.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame should prepare the draw.
  client.task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 1);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  client.task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, BeginRetroFrame_SwapThrottled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // To test swap ack throttling, this test disables automatic swap acks.
  scheduler->SetMaxSwapsPending(1);
  client.SetAutomaticSwapAck(false);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrame", client);
  client.Reset();

  // Create a BeginFrame with a long deadline to avoid race conditions.
  // This is the first BeginFrame, which will be handled immediately.
  BeginFrameArgs args = CreateBeginFrameArgsForTesting();
  args.deadline += base::TimeDelta::FromHours(1);
  scheduler->BeginFrame(args);
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Queue BeginFrame while we are still handling the previous BeginFrame.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  args.frame_time += base::TimeDelta::FromSeconds(1);
  scheduler->BeginFrame(args);
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  // NotifyReadyToCommit should trigger the pending commit and draw.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Swapping will put us into a swap throttled state.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // While swap throttled, BeginRetroFrames should trigger BeginImplFrames
  // but not a BeginMainFrame or draw.
  scheduler->SetNeedsCommit();
  client.task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 1);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Queue BeginFrame while we are still handling the previous BeginFrame.
  args.frame_time += base::TimeDelta::FromSeconds(1);
  scheduler->BeginFrame(args);
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Take us out of a swap throttled state.
  scheduler->DidSwapBuffersComplete();
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 0, 1);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  scheduler->SetNeedsRedraw();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();
}

void BeginFramesNotFromClient(bool begin_frame_scheduling_enabled,
                              bool throttle_frame_production) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.begin_frame_scheduling_enabled =
      begin_frame_scheduling_enabled;
  scheduler_settings.throttle_frame_production = throttle_frame_production;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame
  // without calling SetNeedsBeginFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_FALSE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_actions_());
  client.Reset();

  // When the client-driven BeginFrame are disabled, the scheduler posts it's
  // own BeginFrame tasks.
  client.task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // If we don't swap on the deadline, we wait for the next BeginFrame.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame should prepare the draw.
  client.task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 1);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  client.task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  // Make sure SetNeedsBeginFrame isn't called on the client
  // when the BeginFrame is no longer needed.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, SyntheticBeginFrames) {
  bool begin_frame_scheduling_enabled = false;
  bool throttle_frame_production = true;
  BeginFramesNotFromClient(begin_frame_scheduling_enabled,
                           throttle_frame_production);
}

TEST(SchedulerTest, VSyncThrottlingDisabled) {
  bool begin_frame_scheduling_enabled = true;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient(begin_frame_scheduling_enabled,
                           throttle_frame_production);
}

TEST(SchedulerTest, SyntheticBeginFrames_And_VSyncThrottlingDisabled) {
  bool begin_frame_scheduling_enabled = false;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient(begin_frame_scheduling_enabled,
                           throttle_frame_production);
}

void BeginFramesNotFromClient_SwapThrottled(bool begin_frame_scheduling_enabled,
                                            bool throttle_frame_production) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.begin_frame_scheduling_enabled =
      begin_frame_scheduling_enabled;
  scheduler_settings.throttle_frame_production = throttle_frame_production;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // To test swap ack throttling, this test disables automatic swap acks.
  scheduler->SetMaxSwapsPending(1);
  client.SetAutomaticSwapAck(false);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_FALSE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_actions_());
  client.Reset();

  // Trigger the first BeginImplFrame and BeginMainFrame
  client.task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 1, 2);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // NotifyReadyToCommit should trigger the pending commit and draw.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // Swapping will put us into a swap throttled state.
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // While swap throttled, BeginFrames should trigger BeginImplFrames,
  // but not a BeginMainFrame or draw.
  scheduler->SetNeedsCommit();
  client.task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 1);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // Take us out of a swap throttled state.
  scheduler->DidSwapBuffersComplete();
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client, 0, 1);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  scheduler->SetNeedsRedraw();
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, SyntheticBeginFrames_SwapThrottled) {
  bool begin_frame_scheduling_enabled = false;
  bool throttle_frame_production = true;
  BeginFramesNotFromClient_SwapThrottled(begin_frame_scheduling_enabled,
                                         throttle_frame_production);
}

TEST(SchedulerTest, VSyncThrottlingDisabled_SwapThrottled) {
  bool begin_frame_scheduling_enabled = true;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient_SwapThrottled(begin_frame_scheduling_enabled,
                                         throttle_frame_production);
}

TEST(SchedulerTest,
     SyntheticBeginFrames_And_VSyncThrottlingDisabled_SwapThrottled) {
  bool begin_frame_scheduling_enabled = false;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient_SwapThrottled(begin_frame_scheduling_enabled,
                                         throttle_frame_production);
}

}  // namespace
}  // namespace cc
