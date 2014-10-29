#ifndef LOCALREPORT_H
#define LOCALREPORT_H

#include <string>
#include <utility>

#include "metrics.h"

typedef std::pair<std::string, int> Report;

void receive_reports(int port, std::function<void(const Report&)> handler);

class LocalReport : public Metrics {
public:
  const int port;

  LocalReport(int port,
          std::string service_name,
          std::string category)
    : Metrics(service_name, category), port(port) {}

  virtual void send_metric(const std::string& key, int value) const;
};

#endif
