#ifndef METRICS_H
#define METRICS_H

#include <boost/assign/list_of.hpp>
#include <vector>

static const std::string FilesToShip        ("barn_files_to_ship");
static const std::string FailedToGetSyncList("barn_failed_to_get_sync_list");
static const std::string FullDirectoryShip  ("barn_shipping_entire_log_directory");
static const std::string RotatedDuringShip  ("barn_rotated_during_ship");
static const std::string LostDuringShip     ("barn_lost_during_ship");
static const std::string NumFilesShipped    ("barn_files_shipped");
static const std::string TimeSinceSuccess   ("time_since_success");
static const std::string FailedOverAgents   ("failed_over_agents");

// These metrics will be published as zero if not reported
// (it is necessary for ganlia that values be zeroed).
static const std::vector<std::string> DefaultZeroMetrics =
  boost::assign::list_of(FilesToShip)
                        (FailedToGetSyncList)
                        (FullDirectoryShip)
                        (RotatedDuringShip)
                        (NumFilesShipped)
                        (LostDuringShip)
                        (FullDirectoryShip)
                        (FailedOverAgents);

/*
 * Interface for metrics sending.
 */
class Metrics {
  public:
  const std::string service_name;
  const std::string category;

  Metrics(std::string service_name,
          std::string category)
    : service_name(service_name),
      category(category) {}

  virtual void send_metric(const std::string& key, int value) const = 0;

  virtual ~Metrics() {}
};

/**
 * Used to disable metrics sending.
 */
class NoOpMetrics : public Metrics {
public:
  NoOpMetrics() : Metrics("", "") {}
  virtual void send_metric(const std::string& key, int value) const override {}
};

#endif

