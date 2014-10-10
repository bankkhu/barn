#include <unordered_map>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "barn-agent.h"




/**
 * Unittests for barn-agent.cpp
 */
namespace barn_agent_test
{

using namespace std;
using namespace testing;


auto const SOURCE_DIRECTORY = "/var/log";

auto const TEST_LOG_FILE = "test-file";
auto const LOG_FILE_T0   = "test-file-1";
auto const LOG_FILE_T1   = "test-file-2";
auto const LOG_FILE_T2   = "test-file-3";

static AgentChannel PRIMARY = AgentChannel();
static AgentChannel SECONDARY = AgentChannel();
static const int FAILOVER_INTERVAL = 30;


string file_name_from_path(const string& file_path) {
  return file_path.substr(file_path.find_last_of("\\/")+1, file_path.size());
}

string log_dir(const string& log_file_name) {
  return SOURCE_DIRECTORY + string("/") + log_file_name;
}


class FakeFileOps : public FileOps {
public:
  FileNameList* local_log_files;
  FileNameList* remote_log_files;
  bool shipping_ok = true;

  FakeFileOps() {
    local_log_files = new FileNameList();
    remote_log_files = new FileNameList();
  }

  virtual Validation<FileNameList> log_files_not_on_target(
        const string& source_dir,
        const string& rsync_target) const override {
     FileNameList files_not_on_server;
     set_difference(local_log_files->begin(), local_log_files->end(),
                    remote_log_files->begin(), remote_log_files->end(),
                    inserter(files_not_on_server, files_not_on_server.end()));
     return files_not_on_server;
  }
  virtual bool ship_file(const string& file_path,
                         const string& rsync_target) const override {
     if (shipping_ok)
        remote_log_files->push_back(file_name_from_path(file_path));
     return shipping_ok;
  }

  virtual FileNameList list_log_directory(std::string directory_path) const override {
    return *local_log_files;
  }
  virtual bool wait_for_new_file_in_directory(const std::string& directory, int sleep_seconds) const override {
    return true;
  }
  virtual bool file_exists(std::string file_path) const override {
    string filename = file_name_from_path(file_path);
    return std::find(local_log_files->begin(), local_log_files->end(), filename) != local_log_files->end();
  }
  virtual ~FakeFileOps() {
    delete local_log_files;
    delete remote_log_files;
  }
};


class FakeChannelSelector : public ChannelSelector<AgentChannel> {
public:
  FakeChannelSelector() : ChannelSelector(PRIMARY, SECONDARY, FAILOVER_INTERVAL) {
    last_heartbeat_time = now = 0;
  };

  virtual time_t now_in_seconds() const override {
    return now;
  }

  time_t now;
};


class FakeMetrics : public Metrics {
public:
  unordered_map<string, int> *sent = new unordered_map<string, int>();

  FakeMetrics() : Metrics(0, "", "") {
  };

  virtual void send_metric(const string& key, int value) const {
    (*sent)[key] = (*sent)[key] + value;
  }

  virtual ~FakeMetrics() {
    delete sent;
  }
};


class BarnAgentTest : public Test {
public:

  void SetUp() {
    barn_conf.sleep_seconds = 0;
    PRIMARY.source_dir = "/var/log";
  }

  BarnConf barn_conf;
  NoOpMetrics metrics;
  FakeMetrics recording_metrics;
  FakeChannelSelector channel_selector;
  FakeFileOps fileops;
};


TEST_F(BarnAgentTest, TestNoOp) {
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(0U, fileops.remote_log_files->size());
}

TEST_F(BarnAgentTest, ShipSingleFile) {
  fileops.local_log_files->push_back(TEST_LOG_FILE);
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(1U, fileops.remote_log_files->size());
  EXPECT_EQ(TEST_LOG_FILE, fileops.remote_log_files->at(0));
}

TEST_F(BarnAgentTest, ShipsAllFiles) {
  fileops.local_log_files->push_back(LOG_FILE_T0);
  fileops.local_log_files->push_back(LOG_FILE_T1);
  fileops.local_log_files->push_back(LOG_FILE_T2);
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(3U, fileops.remote_log_files->size());
  EXPECT_EQ(LOG_FILE_T0, fileops.remote_log_files->at(0));
  EXPECT_EQ(LOG_FILE_T1, fileops.remote_log_files->at(1));
  EXPECT_EQ(LOG_FILE_T2, fileops.remote_log_files->at(2));
}

TEST_F(BarnAgentTest, DoesNotShipTwice) {
  fileops.local_log_files->push_back(TEST_LOG_FILE);
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(1U, fileops.remote_log_files->size());
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(1U, fileops.remote_log_files->size());
}

TEST_F(BarnAgentTest, ShipsNewFiles) {
  fileops.local_log_files->push_back(LOG_FILE_T0);

  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(1U, fileops.remote_log_files->size());
  EXPECT_EQ(LOG_FILE_T0, fileops.remote_log_files->at(0));

  fileops.local_log_files->push_back(LOG_FILE_T1);
  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(2U, fileops.remote_log_files->size());
  EXPECT_EQ(LOG_FILE_T1, fileops.remote_log_files->at(1));
}

TEST_F(BarnAgentTest, ShipsLatestFilesOnly) {
  fileops.local_log_files->push_back(LOG_FILE_T0);
  fileops.local_log_files->push_back(LOG_FILE_T1);
  fileops.local_log_files->push_back(LOG_FILE_T2);

  fileops.remote_log_files->push_back(LOG_FILE_T1);

  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(2U, fileops.remote_log_files->size());
  EXPECT_EQ(LOG_FILE_T1, fileops.remote_log_files->at(0));
  EXPECT_EQ(LOG_FILE_T2, fileops.remote_log_files->at(1));
}

TEST_F(BarnAgentTest, ShipFailure) {
  fileops.local_log_files->push_back(TEST_LOG_FILE);
  fileops.shipping_ok = false;

  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  EXPECT_EQ(0U, fileops.remote_log_files->size());
}


class MockFileOps : public FileOps {
public:
  MOCK_CONST_METHOD2(wait_for_new_file_in_directory,
        bool(const string&, int));
  MOCK_CONST_METHOD1(file_exists, bool(string));

  MOCK_CONST_METHOD2(ship_file, bool(const string&, const string&));
  MOCK_CONST_METHOD1(list_log_directory,
        FileNameList(string));
  MOCK_CONST_METHOD2(log_files_not_on_target,
        Validation<FileNameList>(const string&, const string&));
};


class MetricsSendingTest : public BarnAgentTest {
protected:
  virtual void SetUp() {
    BarnAgentTest::SetUp();
    FileNameList log_files;
    log_files.push_back(LOG_FILE_T0);
    log_files.push_back(LOG_FILE_T1);

    ON_CALL(mfileops, list_log_directory(_))
      .WillByDefault(Return(log_files));
    ON_CALL(mfileops, log_files_not_on_target(_, _))
      .WillByDefault(Return(log_files));
    ON_CALL(mfileops, wait_for_new_file_in_directory(_, _))
      .WillByDefault(Return(true));
    ON_CALL(mfileops, ship_file(_, _))
      .WillByDefault(Return(true));
    ON_CALL(mfileops, file_exists(_))
      .WillByDefault(Return(true));

  }
public:
  NiceMock<MockFileOps> mfileops;
};


TEST_F(MetricsSendingTest, TestNoOp) {
  EXPECT_CALL(mfileops, list_log_directory(_))
      .WillOnce(Return(FileNameList()));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(0, (*recording_metrics.sent)[FailedToGetSyncList]);
  EXPECT_EQ(0, (*recording_metrics.sent)[FilesToShip]);
  EXPECT_EQ(0, (*recording_metrics.sent)[FullDirectoryShip]);
  EXPECT_EQ(0, (*recording_metrics.sent)[LostDuringShip]);
  EXPECT_EQ(0, (*recording_metrics.sent)[NumFilesShipped]);
  EXPECT_EQ(0, (*recording_metrics.sent)[RotatedDuringShip]);
}

TEST_F(MetricsSendingTest, TestSuccesfulShip) {
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(2, (*recording_metrics.sent)[FilesToShip]);
  EXPECT_EQ(2, (*recording_metrics.sent)[NumFilesShipped]);
  EXPECT_EQ(1, (*recording_metrics.sent)[FullDirectoryShip]);
}

TEST_F(MetricsSendingTest, TestFailedShip) {
  ON_CALL(mfileops, ship_file(_, _))
      .WillByDefault(Return(false));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(2, (*recording_metrics.sent)[FilesToShip]);
  EXPECT_EQ(0, (*recording_metrics.sent)[NumFilesShipped]);
}

TEST_F(MetricsSendingTest, TestPartialShip) {
  EXPECT_CALL(mfileops, ship_file(log_dir(LOG_FILE_T0), _))
    .WillOnce(Return(true));
  EXPECT_CALL(mfileops, ship_file(log_dir(LOG_FILE_T1), _))
    .WillOnce(Return(false));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(2, (*recording_metrics.sent)[FilesToShip]);
  EXPECT_EQ(1, (*recording_metrics.sent)[NumFilesShipped]);
}

TEST_F(MetricsSendingTest, TestFailedToGetSyncList) {
  EXPECT_CALL(mfileops, log_files_not_on_target(_, _))
    .WillOnce(Return(BarnError("Failed to sync")));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(1, (*recording_metrics.sent)[FailedToGetSyncList]);
}

TEST_F(MetricsSendingTest, TestLostDuringShip) {
  EXPECT_CALL(mfileops, ship_file(log_dir(LOG_FILE_T0), _))
    .WillOnce(Return(false));
  EXPECT_CALL(mfileops, ship_file(log_dir(LOG_FILE_T1), _))
    .WillOnce(Return(true));
  EXPECT_CALL(mfileops, file_exists(log_dir(LOG_FILE_T0)))
    .WillOnce(Return(false));

  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(1, (*recording_metrics.sent)[LostDuringShip]);
  EXPECT_EQ(0, (*recording_metrics.sent)[RotatedDuringShip]);
}

TEST_F(MetricsSendingTest, TestRotatedDuringShip) {
  FileNameList log_files;
  log_files.push_back(LOG_FILE_T0);
  log_files.push_back(LOG_FILE_T1);

  FileNameList missing_log_files;
  missing_log_files.push_back(LOG_FILE_T1);
  EXPECT_CALL(mfileops, list_log_directory(SOURCE_DIRECTORY))
    .WillOnce(Return(log_files))
    .WillOnce(Return(missing_log_files));

  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(0, (*recording_metrics.sent)[LostDuringShip]);
  EXPECT_EQ(1, (*recording_metrics.sent)[RotatedDuringShip]);
}

TEST_F(MetricsSendingTest, TestFullDirectoryShip) {
  FileNameList log_files;
  log_files.push_back(LOG_FILE_T1);

  EXPECT_CALL(mfileops, log_files_not_on_target(_, _))
    .WillOnce(Return(log_files));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(0, (*recording_metrics.sent)[FullDirectoryShip]);

  log_files.clear();
  log_files.push_back(LOG_FILE_T0);
  log_files.push_back(LOG_FILE_T1);
  EXPECT_CALL(mfileops, log_files_not_on_target(_, _))
    .WillOnce(Return(log_files));
  dispatch_new_logs(barn_conf, mfileops, channel_selector, recording_metrics);
  EXPECT_EQ(1, (*recording_metrics.sent)[FullDirectoryShip]);
}


class MockChannelSelector : public ChannelSelector<AgentChannel> {
public:
  MockChannelSelector() : ChannelSelector(PRIMARY, SECONDARY, FAILOVER_INTERVAL) {};

  MOCK_METHOD0(heartbeat, void());
  MOCK_METHOD0(current, AgentChannel());
  MOCK_METHOD0(pick_channel, AgentChannel());
};



class ChannelSelectionTest : public BarnAgentTest {
protected:
  virtual void SetUp() {
    BarnAgentTest::SetUp();
    ON_CALL(m_channel_selector, current())
      .WillByDefault(Return(PRIMARY));
    ON_CALL(m_channel_selector, pick_channel())
      .WillByDefault(Return(PRIMARY));

    FileNameList log_files;
    log_files.push_back(LOG_FILE_T0);
    log_files.push_back(LOG_FILE_T1);
    ON_CALL(mfileops, list_log_directory(_))
          .WillByDefault(Return(log_files));
    ON_CALL(mfileops, log_files_not_on_target(_, _))
        .WillByDefault(Return(log_files));
    ON_CALL(mfileops, wait_for_new_file_in_directory(_, _))
        .WillByDefault(Return(true));
    ON_CALL(mfileops, file_exists(_))
        .WillByDefault(Return(true));
  }

public:
  NiceMock<MockChannelSelector> m_channel_selector;
  NiceMock<MockFileOps> mfileops;
};


TEST_F(ChannelSelectionTest, TestHeartbeatOnNoOp) {
  EXPECT_CALL(m_channel_selector, pick_channel()).WillOnce(Return(PRIMARY));
  EXPECT_CALL(m_channel_selector, heartbeat()).WillOnce(Return());
  dispatch_new_logs(barn_conf, fileops, m_channel_selector, metrics);
}

TEST_F(ChannelSelectionTest, TestHeartbeatSuccessfulShip) {
  fileops.local_log_files->push_back(TEST_LOG_FILE);
  EXPECT_CALL(m_channel_selector, heartbeat()).WillOnce(Return());
  dispatch_new_logs(barn_conf, fileops, m_channel_selector, recording_metrics);
}

// Tests that a 'flacky' channel is considered an active channel.
TEST_F(ChannelSelectionTest, TestHeartbeatPartialShip) {
  // Ship one file, fail the next
  EXPECT_CALL(mfileops, ship_file(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  // Should still heartbeat
  EXPECT_CALL(m_channel_selector, heartbeat()).WillOnce(Return());
  dispatch_new_logs(barn_conf, mfileops, m_channel_selector, recording_metrics);
}

TEST_F(ChannelSelectionTest, TestNoHeartbeatOnFailedShip) {
  // Fail all ships
  EXPECT_CALL(mfileops, ship_file(_, _))
      .WillOnce(Return(false));
  // Should not heartbeat
  EXPECT_CALL(m_channel_selector, heartbeat()).Times(0);
  dispatch_new_logs(barn_conf, mfileops, m_channel_selector, recording_metrics);
}

}  // namespace barn_agent_test
