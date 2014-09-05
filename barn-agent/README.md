Barn Agent
--------

Barn Agent, runs as a part of Barn logging pipeline. It monitors directories containing
rotating log files and upon rotation of a new log file, it ships the file to the Barn Master. 

Barn Agent is written in C++ and requires the following dependencies:

* Compile-time:
  - Boost Libraries (tested against 1.49.0) and needs to link against (boost::system, boost::filesystem)
  - Boost.Process (which is not a part of official boost libraries, but included in the source three.)
  - A C++11 compliant to compile C++11 features. Tested with GCC 4.7.2.
* Run-time:
  - Rsync (tested on 3.0.7 but should work with 3.0.x)
  - inotifywait (which I'll remove the dependency in the future by replacing with system calls)


#### How to build

* Build boost
* Run make:

     ```
     $ make build
      ```

Optionally you can set BOOST_INCLUDE_PATH and BOOST_LIB_PATH if your boost is installed
in default header and library paths.

  On debian, you'll satisfy build requirements by having the following packages installed:
      
     - libboost-dev
     - libboost-system-dev 
     - libboost-filesystem-dev 
     - libboost-timer-dev 
     - libboost-program-options-dev      
      
* You can also make a debian package out of the binary, via ```make dist```.


#### How to test

        $ make test

or to run directly after `make build`:

        ./test/barn_test

to run individual tests:

        ./test/barn_test --gtest_filter=HelpersTest.*

#### How to run

```
./barn-agent BARN_MASTER_HOST:PORT LOCAL_LOG_DIRECTORY NAME_OF_SERVICE CATEGORY
```

e.g.

```
./barn-agent --monitor_mode true --monitor_port 23635
./barn-agent --master 10.99.00.29:11025 --source /etc/service/myapp/log/main --service-name myapp --category main --monitor_port 23635
```

####### Barn Agent is a part of the Barn package and Barn's LICENSE applies.
