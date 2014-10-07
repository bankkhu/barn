#include "gtest/gtest.h"
#include "channel_selector.h"

using namespace std;

class ChannelSelectorTest : public ::testing::Test {
};


static const int PRIMARY = 1;
static const int SECONDARY = 2;

static const int FAILOVER_INTERVAL = 30;

class MockChannelSelector : public ChannelSelector<int> {
public:
  MockChannelSelector() : ChannelSelector(PRIMARY, SECONDARY, FAILOVER_INTERVAL) {
    last_heartbeat_time = now = 0;
  };

  virtual time_t now_in_seconds() const override {
    return now;
  }

  time_t now;
};

TEST_F(ChannelSelectorTest, PrimaryByDefault) {
  MockChannelSelector cs = MockChannelSelector();
  cs.pick_channel();
  EXPECT_EQ(PRIMARY, cs.current());
  cs.now = FAILOVER_INTERVAL/2;
  cs.pick_channel();
  EXPECT_EQ(PRIMARY, cs.current());
}

TEST_F(ChannelSelectorTest, DoNotFailoverWithHeartbeat) {
  MockChannelSelector cs = MockChannelSelector();
  EXPECT_EQ(PRIMARY, cs.current());
  cs.now = FAILOVER_INTERVAL/2;
  cs.heartbeat();
  cs.now = FAILOVER_INTERVAL + 1;
  cs.pick_channel();
  EXPECT_EQ(PRIMARY, cs.current());
}

TEST_F(ChannelSelectorTest, FailoverForNoHeartbeat) {
  MockChannelSelector cs = MockChannelSelector();
  EXPECT_EQ(PRIMARY, cs.current());
  // No: cs.heartbeat() implies no succesful call to primary
  cs.now = FAILOVER_INTERVAL + 1;
  cs.pick_channel();
  EXPECT_EQ(SECONDARY, cs.current());
}

TEST_F(ChannelSelectorTest, FailbackAfterFailure) {
  MockChannelSelector cs = MockChannelSelector();
  EXPECT_EQ(PRIMARY, cs.current());
  cs.now = FAILOVER_INTERVAL + 1;
  cs.pick_channel();
  EXPECT_EQ(SECONDARY, cs.current());
  cs.now = 2 * FAILOVER_INTERVAL + 1;
  cs.pick_channel();
  EXPECT_EQ(PRIMARY, cs.current());
}

TEST_F(ChannelSelectorTest, FullStory) {
  auto T = [](double fraction) -> time_t {return (time_t)(fraction * FAILOVER_INTERVAL);};
  MockChannelSelector cs = MockChannelSelector();
   
  // On primary for a few iterations
  cs.now = T(0.0);
  EXPECT_EQ(PRIMARY, cs.current());
  cs.heartbeat();

  cs.now = T(0.5);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  cs.now = T(1.0);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();
 
  cs.now = T(1.5);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  // - Primary Failure Now -
  cs.now = T(2.0);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  // No heartbeat

  cs.now = T(2.49);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  // No heartbeat

  // - Should Fail Over To Secondary -
  cs.now = T(2.51);
  EXPECT_EQ(SECONDARY, cs.pick_channel());
  cs.heartbeat();  // heartbeat should not affect anything while on secondary

  cs.now = T(3.39);
  EXPECT_EQ(SECONDARY, cs.pick_channel());
  cs.heartbeat();  // heartbeat should not affect anything while on secondary

  // - Should Try Primary -
  cs.now = T(3.52);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();  // Found primary ok

  // Should fail back to primary after some period on secondary
  cs.now = T(4.01);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  // Should stay on primary if heartbeat ok
  cs.now = T(4.5);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  cs.now = T(5.0);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  cs.now = T(5.5);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
  cs.heartbeat();

  cs.now = T(6.0);
  EXPECT_EQ(PRIMARY, cs.pick_channel());
}


