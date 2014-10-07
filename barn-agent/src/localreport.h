#ifndef LOCALREPORT_H
#define LOCALREPORT_H

#include <string>
#include <iostream>
#include <string>
#include <boost/assign/list_of.hpp>

const std::string FilesToShip        ("barn_files_to_ship");
const std::string FailedToGetSyncList("barn_failed_to_get_sync_list");
const std::string RotatedDuringShip  ("barn_rotated_during_ship");
const std::string LostDuringShip     ("barn_lost_during_ship");
const std::string NumFilesShipped    ("barn_files_shipped");

//These metrics will be published as zero if not occured
const std::vector<std::string> DefaultZeroMetrics =
  boost::assign::list_of(FilesToShip)
                        (FailedToGetSyncList)
                        (RotatedDuringShip)
                        (NumFilesShipped)
                        (LostDuringShip);

/*
 * Used by barn-agent to emit telemetry.
 */
class Metrics {
  public:

  const int port;
  const std::string service_name;
  const std::string category;

  Metrics(int port,
          std::string service_name,
          std::string category)
    : port(port),
      service_name(service_name),
      category(category)
  {};

  virtual void send_metric(const std::string& key, int value) const;
};

/**
 * Used to disable metrics sending.
 */
class NoOpMetrics : public Metrics {
public:
  NoOpMetrics() : Metrics(0, "", "") {};
  virtual void send_metric(const std::string& key, int value) const override {};
};


/*
 * Used by barn-monitor to receive telemtry.
 * TODO: refactor.
 */
class Report {
  public:
  const std::string service_name;
  const std::string category;
  const std::string key;
  const int value;

  Report(std::string service_name,
         std::string category,
         std::string key,
         int value)
    : service_name(service_name),
      category(category),
      key(key),
      value(value)
  {};

  static Report deserialize(const std::string& serialized);
};

void receive_reports(int port, std::function<void(const Report&)> handler);

void send_datagram(int port, std::string message);

template<int buffer_size = 250>
void receive_datagrams(int port, std::function<void(const std::string&)> handler);

std::pair<std::string, int> kv_pair(const Report& report);

#endif
