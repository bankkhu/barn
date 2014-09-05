#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread/mutex.hpp>

#include "barn-agent-monitor.h"
#include "ganglia.h"
#include "localreport.h"

using namespace std;
using boost::posix_time::seconds;
using boost::bind;
using boost::asio::io_service;

typedef boost::asio::deadline_timer Timer;
typedef boost::circular_buffer<pair<string,int>> MetricRepo;

map<string, int> aggregate(boost::circular_buffer<pair<string,int>> buffer);

const int report_interval = 30; //seconds
const int max_metrics_per_interval = 1000;

void addDefaultMetrics(MetricRepo& metrics) {
  for(auto& el : DefaultZeroMetrics)
    metrics.push_back(make_pair(el, 0));
}

void timer_action(Timer* timer, MetricRepo* metrics, boost::mutex* repo_mutex){
  MetricRepo metrics_copy;

  {
    boost::mutex::scoped_lock lock(*repo_mutex);
    metrics_copy = *metrics;
    metrics->clear();
    addDefaultMetrics(*metrics);
  }

  for(auto &m : aggregate(metrics_copy)) {
    cout << "Reporting externally: " << m.first << " -> " << m.second << endl;
    report_ganglia(metric_group, m.first, m.second);
  }

  timer->expires_at(timer->expires_at() + seconds(report_interval));
  timer->async_wait(bind(timer_action, timer, metrics, repo_mutex));
}

void barn_agent_local_monitor_main(const BarnConf& barn_conf) {

  auto metrics = MetricRepo(max_metrics_per_interval);
  boost::mutex mutex;

  io_service io;
  Timer timer(io, seconds(0));
  timer.async_wait(bind(timer_action, &timer, &metrics, &mutex));
  std::thread t(boost::bind(&io_service::run, &io));

  receive_reports(barn_conf.monitor_port, [&](const Report& new_report) {
    boost::mutex::scoped_lock lock(mutex);
    metrics.push_back(kv_pair(new_report));
    // cout << "Individual report received: " << new_report.serialize() << endl;
  });

}

map<string, int> aggregate(boost::circular_buffer<pair<string,int>> buffer) {
  map<string,int> aggregated;

  for(auto element : buffer)
    aggregated[element.first] += element.second;

  return aggregated;
}

