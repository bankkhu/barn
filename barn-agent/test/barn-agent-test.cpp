#include "gtest/gtest.h"

#include "barn-agent.h"

using namespace std;


/**
 * Unittests for barn-agent.cpp
 */
class BarnAgentTest : public ::testing::Test {
};

static const AgentChannel PRIMARY = AgentChannel();
static const AgentChannel SECONDARY = AgentChannel();
static const int FAILOVER_INTERVAL = 30;


class FakeFileOps : public FileOps {
public:
  virtual Validation<FileNameList> log_files_not_on_target(
        const string& source_dir,
        const string& rsync_target) const override {
     FileNameList files_not_on_server;
     files_not_on_server.push_back("test_file");
     return files_not_on_server;
  }
  virtual bool ship_file(const std::string& file_name,
                         const std::string& rsync_target) const override {
     return true;
  }

  virtual std::vector<std::string> list_log_directory(std::string directory_path) const override {
    std::vector<std::string> files;
    files.push_back("test_file");
    return files;
  }
  virtual int wait_for_new_file_in_directory(const std::string& directory, int sleep_seconds) const override {
    cout << "calling inotifywait" << endl;
    return 0;
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


// Simple dummy test for now.
TEST_F(BarnAgentTest, BasicTestForNow) {
  BarnConf barn_conf = BarnConf();
  NoOpMetrics metrics = NoOpMetrics();
  FakeChannelSelector channel_selector = FakeChannelSelector();
  FakeFileOps fileops = FakeFileOps();

  dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
}

