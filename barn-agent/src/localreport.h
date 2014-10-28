#ifndef LOCALREPORT_H
#define LOCALREPORT_H

#include <string>
#include <iostream>
#include <string>
#include <boost/assign/list_of.hpp>

#include "params.h"

const std::string FilesToShip        ("barn_files_to_ship");
const std::string FailedToGetSyncList("barn_failed_to_get_sync_list");
const std::string FullDirectoryShip  ("barn_shipping_entire_log_directory");
const std::string RotatedDuringShip  ("barn_rotated_during_ship");
const std::string LostDuringShip     ("barn_lost_during_ship");
const std::string NumFilesShipped    ("barn_files_shipped");
const std::string TimeSinceSuccess   ("time_since_success");
const std::string FailedOverAgents   ("failed_over_agents");

// These metrics will be published as zero if not reported
// (it is necessary for ganlia that values be zeroed).
const std::vector<std::string> DefaultZeroMetrics =
  boost::assign::list_of(FilesToShip)
                        (FailedToGetSyncList)
                        (FullDirectoryShip)
                        (RotatedDuringShip)
                        (NumFilesShipped)
                        (LostDuringShip)
                        (FullDirectoryShip)
                        (FailedOverAgents);

/*
 * Used by barn-agent to emit telemetry.
 */
class Metrics {
  public:

  const int port;
  const std::string service_name; // Currently unused (reporting done per machine).
  const std::string category;     //    "

  Metrics(int port,
          std::string service_name,
          std::string category)
    : port(port),
      service_name(service_name),
      category(category)
  {};

  virtual void send_metric(const std::string& key, int value) const;

  virtual ~Metrics() {};
};

/**
 * Used to disable metrics sending.
 */
class NoOpMetrics : public Metrics {
public:
  NoOpMetrics() : Metrics(0, "", "") {};
  virtual void send_metric(const std::string& key, int value) const override {};
};


typedef std::pair<std::string, int> Report;

inline Metrics* create_metrics(const BarnConf& barn_conf) {
  if (barn_conf.monitor_port > 0) {
    return new Metrics(barn_conf.monitor_port, barn_conf.service_name,
                       barn_conf.category);
  } else {
    return new NoOpMetrics();
  }
}

void receive_reports(int port, std::function<void(const Report&)> handler);
#endif
