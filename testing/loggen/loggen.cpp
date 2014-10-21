#include "stdio.h"

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace std;

//Code copied from http://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
void gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

// http://timmurphy.org/2009/09/29/nanosleep-in-c-c/
void sleep_fraction(float fraction_of_second) {
  int milisec = fraction_of_second * 1000; // length of time to sleep, in miliseconds
  struct timespec req = {0};
  req.tv_sec = 0;
  req.tv_nsec = milisec * 1000000L - 1; //Minus one not to exceed 999999999
  nanosleep(&req, (struct timespec *)NULL);
}

int main(int argc, char* argv[]) {

  if(argc < 2) {
    cerr << "Usage: loggen MB_OF_LOGS_TO_PRODUCE_PER_SEC" << endl;
    exit(0);
  }
  const int max_len = 500;
  char* buffer = new char[max_len];
  int len = 0;
  double mb = atof(argv[1]);

  cout << "Writing " << mb << " MB of random lines to stdout per sec" << endl;


  unsigned long seq_num = 0;
  int max_bytes = int(mb * 1024 * 1024);

  while(true) {
    const clock_t begin_time = clock();

    int made_bytes = 0;

    while(made_bytes <= max_bytes) {
      len = rand() % (max_len - 1);
      gen_random(buffer, len);
      printf("%10lu ", seq_num++);
      cout << buffer << "\n";
      made_bytes += len + 12;
    }

    const clock_t end_time = clock();
    const float took = float( end_time - begin_time ) /  CLOCKS_PER_SEC;

    sleep_fraction(max(0., 1.0 - took));
  }

}
