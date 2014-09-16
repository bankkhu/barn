#!/usr/bin/env bash

apt-get update
apt-get install -y g++
apt-get install -y scons
apt-get install -y libboost-dev libboost-system-dev libboost-filesystem-dev  libboost-timer-dev libboost-program-options-dev
apt-get install -y openjdk-6-jdk
apt-get install -y rubygems
apt-get install -y git
apt-get install -y inotify-tools
apt-get install -y gdb
apt-get install -y daemontools  # tai64n command is useful

echo "export JAVA_HOME=/usr/lib/jvm/java-6-openjdk-amd64" >> /home/vagrant/.bashrc

gem install fpm
