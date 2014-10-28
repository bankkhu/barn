#include "localreport.h"

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <sstream>
#include <boost/bind.hpp>

using namespace std;

static void send_datagram(int port, std::string message);
static void receive_datagrams(int port, function<void(const string&)> handler);

void Metrics::send_metric(const std::string& key, int value) const {
  static const auto SERIALIZATION_DELIM = ' ';
  std::ostringstream oss;

  oss << key << SERIALIZATION_DELIM << value;

  send_datagram(port, oss.str());
}

static Report deserialize(const std::string& serialized) {
  std::istringstream iss(serialized);

  string key;
  int value;

  iss >> key >> value;

  return Report(key, value);
}

void receive_reports(int port, function<void(const Report&)> handler) {
  receive_datagrams(port, bind(handler, bind(deserialize, _1)));
}

void send_datagram(int port, std::string message) {
  using namespace boost;
  using namespace boost::asio;
  using namespace boost::asio::ip;

  io_service io_service;
  udp::socket socket(io_service);
  socket.open(udp::v4());
  auto end_point = udp::endpoint(address(address_v4::loopback()), port);
  socket.send_to(boost::asio::buffer(message.c_str(), message.size()), end_point);
}

void receive_datagrams(int port, function<void(const string&)> handler) {
  using namespace boost;
  using namespace boost::asio;
  using namespace boost::asio::ip;
  static const int buffer_size = 250;

  io_service io_service;
  udp::socket socket(io_service, udp::endpoint(udp::v4(), port));
  boost::array<char, buffer_size> recv_buf;
  udp::endpoint remote_endpoint;

  while(true) {
    auto size_read = socket.receive_from(buffer(recv_buf), remote_endpoint);
    std::string datagram(recv_buf.begin(), recv_buf.begin() + size_read);
    handler(datagram);
  }
}

