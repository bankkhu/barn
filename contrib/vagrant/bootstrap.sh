#!/usr/bin/env bash

apt-get update
apt-get install -y g++
apt-get install -y scons
apt-get install -y libboost-dev libboost-system-dev libboost-filesystem-dev  libboost-timer-dev libboost-program-options-dev
apt-get install -y openjdk-6-jdk
apt-get install -y rubygems
gem install fpm
