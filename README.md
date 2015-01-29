# Barn

Barn is a log aggregation and archival system.

## Overview

Barn attempts to solve the following problem:
provide high throughput, fault-tolerent shipping & concatenation of
log files from application machines to HDFS.

In contrast to other systems, which attempt to treat log aggregation as a
messaging problem, Barn treats it as a file transfer problem.
(yes)

### Diagram

![Barn Diagram](/contrib/barn_diagram.png?raw=true)

### Producer

We assume the producer is a process emitting newline-separated character data
on standard output. Every line is considered a log record, and gets written to
the local disk using the excellent `svlogd` program from the `runit` package.
A separate process 'barn-agent' watches `svlogd`s output directory, and, upon
log file rotation (using inotify api), attempts to ship non-current log files
to a remote server.

```shell
# example snippet of a runit 'log/run' script

barn-harvester-agent farm.acme.org:1025 ./main my-service main &
svlogd -tt ./main
```

It is also trivial to intercept the standard output stream using a simple UNIX
pipe and, for example, split it into multiple topic streams, each handled by a
separate `svlogd`:

```shell
# (setup barn-harvester-agent ...)

# capture stdout and split into two streams
tee >( grep "^ERROR" | svlogd -tt ./errors  ) \
    >( grep -v "^ERROR" | svlogd -tt ./main )
    > /dev/null
```

Now, even though we can tweak `svlogd`s rotation policy, we have to wait until a
certain amount of data has been written, until it gets shipped to remote
storage.


### Collector: barn-agent

The destination for `barn-harvester-agent` doesn't do a lot. In fact, the
current implementation is just a `rsync` daemon (so `barn-harvester-agent` is
only little more than a clever way to invoke `rsync`). We also provide some
script to keep a slave `rsync` server in sync (failover of the producers is not
automatic).

### Aggregator: barn-hdfs

The collector node is assumed to be a few central machines, that concatenates
files and ships them to distributed storage -- we use HDFS.
Since importing data into HDFS in a reliable, fault-tolerant way
is a bit of a hairy thing, we provide `barn-hdfs`, which exploits the `Tai64`
timestamps `svlogd` uses to name rotated logfiles to provide idempotent,
block-optimizing writes to HDFS.

## How barn works

To get logs from a source application (e.g. a webserver) to HDFS barn is setup
as follows;

* the source application logs to stdout and is wrapped in [svlogd](http://smarden.org/runit/svlogd.8.html).
* when svlogd rotates the file it creates a file named by the [tai64n](http://cr.yp.to/libtai/tai64.html) time.
* a barn-agent per source application uses [inotify](http://man7.org/linux/man-pages/man7/inotify.7.html) to be alerted when a new file is created.
* 'barn-agent' then rsyncs the file to a 'barn-hdfs' machine.
* 'barn-hdfs' takes care of storing these logs for a while, merging them and
shipping them to HDFS.
* 'barn-hdfs' either waits until the files are greater than 1GB in size or a
'shipping-interval' has passed, then merges and compresses whatever files are available.
* It then copies this file to tmp directory on HDFS and does an atomic rename to
its destination directory on HDFS.

## Related Work

* syslog + Flume

This is the combination we've been running in production for about a year. The
main problem was around `append`ing to HDFS files, which is very sensitive to
cluster availability -- even restarting data nodes could confuse the DFSClient.
We also had to write a lot of custom code (disk buffering, sequence files,
syslog parsing to get the desired bucketing), which would need to be rewritten
for `flume-ng`.

While `syslog` mainly does what you would expect, it is archaic to configure, and doesn't tell you anything if it happens to _not_ do what you expect.

* AMQP

For a while, we were obsessed with the idea that we could use our existing
RabbitMQ infrastructure and experience, and treat logging as just a very high
throughput messaging problem. But we couldn't get anywhere near the desired
performance.

_TODO: wasn't there some OSS project which uses AMQP?_

* Scribe

Facebook's Scribe essentially implements what `barn-agent` does. It does it
well, or so we hear, but we would still have had to implement the other parts
ourselves.  Another issue is that Scribe speaks Thrift, over TCP, exclusively.
Which, apart from more involved integration in heterogenous environments, would
require the `Haystacker` to parse, inspect and transform log files -- we can't
and don't want to use Thrift for long-term archival, and tailor all our
workflows around that.

* Kafka

LinkedIn's Kafka is a very interesting project, so we looked at it very hard.
And from different angles. And with 1-6 eyes. Eventually, we decided to not use
it, because it seemed too much of an operational overhead to keep it running
reliably and with high throughput. Namely:

  * Kafka relies heavily on ZooKeeper, which is hard to operate
  * as we understand, a single Kafka broker shouldn't serve a large number of
    producers: due to the non-reliable messaging protocol Kafka implements, if a
    broker goes down, all messages currently in-flight will be lost (most likely
    more, depending on the client's network implementation). This might be fine,
    eg. if you're anyway planning to dedicate a machine per rack to collect and
    forward logs, and if you can afford to loose a couple of messages here and
    there. In our setup, however, it isn't.
  * Kafka uses a custom binary wire protocol, which is also used for disk
    persistence. That's nice, but we again would have to parse it before writing
    to HDFS.

That being said, `barn`s design is heavily influenced by Kafka, except that we
got rid of coordination (mostly) and that we push the "queue history" idea to
the producer itself (as well as the collector).

* Others

There are also a couple of other systems out there, eg. Heroku's logplex, or
logstash. They seem to be targeted at very different scenarios though, and
wouldn't satisfy all of our requirements. YMMV, as usual.


## Build

Make sure you have the following installed on the build system:

* a recent gcc (version >= 4.7 for C++11)
* a recent JDK
* a working `make`
* [SCons](http://www.scons.org/)

Then, in the top level of the repository, simply type:

```shell
make dist
```

## Installation

We provide debian packages of all `barn` components (_TODO_), as well as zip
and tar packages. The runtime dependencies are as follows:

### Agent

runit, rsync, inotify, bash, (grep, awk), optional: contrib/rtail

### Collector

rsync, bash

### Haystacker

JRE >= 6, and a HDFS cluster to talk to


We recommend to run the `Collector` and `Haystacker` as `runit` services.


## Configuration

All `barn` executables are configured through command line parameters. Where
`svlogd` is used, refer to `man svlogd` for additional file based configuration
options.

Some knobs to turn are:

### Agent

* you may want to adjust the rotation / history policy for `svlogd` to the
  amount of disk space you have available
* for low-volume producers, you may want to set the rotation policy for `svlogd`
  to a low value
* for high-volume producers, you may want to mount a RAM disk for `svlogd` to
  write to. This will allow the producer to continue functioning even if the
  machine's disk crashes, giving up a little bit of safety in case the system
  needs to be power-cycled.
* a RAM disk may also be an option if you can't write to local disk at all

### Collector / Haystacker

It's all about RAID and disk partitioning. Heavily depends on what you want to
optimize for. Make sure you can use as much bandwidth as possible to talk to
HDFS (possibly use two NICs on two switches for ingress and egress
respectively).

### Failover

At this point, we don't have any automatic failover mechanism for `barn-agent`
in case the collector is not reachable. We recommend to use a DNS entry with a
short TTL and perform manual failover.

## Maintainers

  * Emily Green <em@soundcloud.com>
  * Sean Braithwaite <brapse@soundcloud.com>

## Contributors

  * Omid Aladini <omid.aladini@gmail.com>
  * Connor Hennessy <conor@soundcloud.com>
  * Kim Altintop [@kim](http://github.com/kim)
  * Brendan Hay [@brendanhay](http://github.com/brendanhay)
  * Torsten Curdt <tcurdt@vafer.org>


