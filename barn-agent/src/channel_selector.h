#ifndef CHANNEL_SELECTOR_H
#define CHANNEL_SELECTOR_H

#include "metrics.h"
#include "params.h"


template <class T> class ChannelSelector {
public:
  virtual void heartbeat() = 0;
  virtual T pick_channel() = 0;
  virtual T current() const = 0;
  virtual void send_metrics(const Metrics&) const {}

  virtual ~ChannelSelector() {};
};

template <class T> class SingleChannelSelector : public ChannelSelector<T> {
public:
  SingleChannelSelector(T channel):
        channel(channel) {}

  virtual void heartbeat() override {}
  virtual T pick_channel() override { return channel; }
  virtual T current() const override { return channel; }

private:
  T channel;
};


/*
 * Time based channel selector for choosing between primary and
 * secondary (backup) endpoints.
 * A heartbeat is used to keep a channel alive and by calling it within
 * 'seconds_before_failover' time.
 *
 * The channel picker will alternate between broken primary and secondary
 * every 'seconds_before_failover'. So if a primary channel recovers it
 * wil be reused again.
 *
 * Example Usage:
 *     cs = C
 *     while (true)
 *         channel = cs.pick_channel()
 *         // or
 *         if (primary_channel_ok)
 *             cs.heartbeat()
 */
template <class T> class FailoverChannelSelector : public ChannelSelector<T> {

public:
    FailoverChannelSelector(T primary, T secondary, int seconds_before_failover):
        primary(primary),
        secondary(secondary),
        seconds_before_failover(seconds_before_failover) {
      assert(seconds_before_failover > 0);
      primary_ok = true;
      last_heartbeat_time = now_in_seconds();
   }

  virtual T current() const override {
    if (primary_ok)
        return primary;
    else
        return secondary;
  }

  virtual void send_metrics(const Metrics& m) const override {
    m.send_metric(TimeSinceSuccess, now_in_seconds() - last_heartbeat_time);
    if (!primary_ok)
        m.send_metric(FailedOverAgents, 1);
  }

  virtual void heartbeat() override {
    if(primary_ok) {
        last_heartbeat_time = now_in_seconds();
    }
  }

  virtual T pick_channel() override {
    time_t now = now_in_seconds(); 
    time_t time_since_heartbeat = now - last_heartbeat_time;
    if (primary_ok &&
        time_since_heartbeat < seconds_before_failover) {
      // normal case, everything ok
    } else if (primary_ok) {
      // too long, perform failover
      LOG (ERROR) << "!!Channel: error primary down for too long, failing to backup";
      primary_ok = false;
      last_heartbeat_time = now;
    // TODO: set failback seconds independent of failover
    } else if (time_since_heartbeat < seconds_before_failover) {
      // on secondary, stay there for now
    } else {
      // on secondary for long enough, try primary again
      LOG (WARNING) << "!!Channel: trying to fail back to primary from backup";
      primary_ok = true;
      last_heartbeat_time = now;
    }
    return current();
  }

  virtual time_t now_in_seconds() const {
    time_t t = time(0);
    if (t < 0) {
        throw "Failed to get time from system";
    }
    return t;
  }

protected:
  // On primary channel: time since last heartbeat() function call
  // On secondary channel: time since flip from primary to secondary
  // protected for testing.
  time_t last_heartbeat_time;

private:
    T primary, secondary;
    bool primary_ok;
    time_t seconds_before_failover;
};

#endif  // CHANNEL_SELECTOR_H
