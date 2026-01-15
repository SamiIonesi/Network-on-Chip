#include <systemc.h>
#include "router.h"
#include <iostream>

using namespace std;

int sc_main(int argc, char* argv[]) {
    Router r1("Router1");

    sc_fifo<packet> fifo_in[4];
    sc_fifo<packet> fifo_out[4];
    sc_fifo<cfg_trans> fifo_cfg;

    r1.cfg_port(fifo_cfg);
    for (int i = 0; i < 4; i++) {
        r1.in_ports[i](fifo_in[i]);
        r1.out_ports[i](fifo_out[i]);
    }

    cout << "--- START L0  ---" << endl;

    // 1. CONFIG: Dest 10 -> Nord(0), Dest 20 -> Est(2)
    fifo_cfg.write(cfg_trans(cfg_trans::SET_ROUTE, 10, 0)); 
    fifo_cfg.write(cfg_trans(cfg_trans::SET_ROUTE, 20, 2));
    fifo_cfg.write(cfg_trans(cfg_trans::SET_ROUTE, 35, 0)); 
    fifo_cfg.write(cfg_trans(cfg_trans::SET_ROUTE, 120, 3));
    fifo_cfg.write(cfg_trans(cfg_trans::ENABLE_PORT, V, 0));

    // 0 = Fixed Priority, 1 = Round Robin
    // fifo_cfg.write(cfg_trans(cfg_trans::SET_ARBITER, 0, 1)); 

    sc_start(20, SC_NS); 

    // 2. DATA
    
    cout << "Injecting packet 1 (on Port 3)..." << endl;
    // REQ_WRITE, Src:99, Dst:10, Addr:0, Data:100
    fifo_in[V].write(packet(packet::REQ_WRITE, 99, 10, 0, 100));

    cout << "Injecting packet 2 (on Port 2)..." << endl;
    // REQ_WRITE, Src:20, Dst:34, Addr:0, Data:100
    fifo_in[E].write(packet(packet::REQ_WRITE, 20, 34, 0, 100)); 
    
    cout << "Injecting packet 3 (on Port 0 - HIGH PRIORITY)..." << endl;
    // REQ_WRITE, Src:88, Dst:20, Addr:5, Data:500
    fifo_in[N].write(packet(packet::REQ_WRITE, 88, 20, 5, 500));

    cout << "Injecting packet 4 (on Port 1)..." << endl;
    // REQ_WRITE, Src:120, Dst:35, Addr:8, Data:40
    fifo_in[S].write(packet(packet::REQ_WRITE, 120, 35, 8, 40));

    cout << "Injecting packet 5 (on Port 1)..." << endl;
    // REQ_WRITE, Src:1, Dst:120, Addr:12, Data:43
    fifo_in[S].write(packet(packet::REQ_WRITE, 1, 120, 12, 43));


    sc_start(100, SC_NS); 

    // 3. CHECK
    packet p_out;
    
    if (fifo_out[N].nb_read(p_out)) {
        cout << "Received on NORTH (Port 0): " << p_out << endl;
    } else {
        cout << "NORTH (Port 0) is empty." << endl;
    }

    if (fifo_out[S].nb_read(p_out)) {
        cout << "Received on SOUTH (Port 1): " << p_out << endl;
    } else {
        cout << "SOUTH (Port 1) is empty." << endl;
    }

    if (fifo_out[E].nb_read(p_out)) {
        cout << "Received on EAST (Port 2): " << p_out << endl;
    } else {
        cout << "EAST (Port 2) is empty." << endl;
    }

    if (fifo_out[V].nb_read(p_out)) {
        cout << "Received on WEST (Port 3): " << p_out << endl;
    } else {
        cout << "WEST (Port 3) is empty." << endl;
    }
    cout << "--- END ---" << endl;
    return 0;
}