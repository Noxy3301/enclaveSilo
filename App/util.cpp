#include "../Include/consts.h"
#include "../Include/util.h"

#include <iostream>

using std::cout;
using std::endl;

void displayParameter() {
    cout << "=====Parameters=====" << endl;   // 20
    cout << "#clocks_per_us:\t" << CLOCKS_PER_US << endl;
    cout << "#epoch_time:\t" << EPOCH_TIME << endl;
    cout << "#extime:\t" << EXTIME << endl;
    cout << "#max_ope:\t" << MAX_OPE << endl;
    cout << "#rmw:\t\t" << RMW << endl;
    cout << "#rratio:\t" << RRAITO << endl;
    cout << "#thread_num:\t" << THREAD_NUM << endl;
    cout << "#tuple_num:\t" << TUPLE_NUM << endl;
    cout << "#ycsb:\t\t" << YCSB << endl;
    cout << "#zipf_skew:\t" << ZIPF_SKEW << endl;
    cout << "#logger_num:\t" << LOGGER_NUM << endl;
    cout << "#buffer_num:\t" << BUFFER_NUM << endl;
    cout << "#buffer_size:\t" << BUFFER_SIZE << endl;
    cout << "====================" << endl;   // 20
}