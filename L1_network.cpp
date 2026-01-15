#include <systemc.h>
#include <vector>
#include <string>
#include "utils.h"
#include "router.h"
#include "cpu_v1.h"
#include "mem.h"

// funtie pt a genera nume diferite pentru fiecare modul
std::string gen_name(const char* prefix, int id) {
    char buf[20];
    sprintf(buf, "%s_%d", prefix, id);
    return std::string(buf);
}

SC_MODULE(Network) {

    Router* routers[8]; // 8 routere în rețea
    std::vector<CPU*> cpus; // stocam referintele catre CPU-uri pt ca nu stim cate o sa avem
    std::vector<MEM*> mems;

    // Avem 7 segmente între 8 routere. Fiecare segment e dublu (dus-întors).
    sc_fifo<packet>* link_fwd[7]; // Est -> Vest
    sc_fifo<packet>* link_bwd[7]; // Vest -> Est

    // Cabluri pentru periferice (CPU/MEM) si porturi închise, le salvez in vector sa nu se piarda referintele 
    std::vector<sc_fifo<packet>*> periph_fifos;

    // Porturile de configurare pentru fiecare router
    sc_fifo_in<cfg_trans> cfg_ports[8];

    SC_CTOR(Network) {
        // Instantierea routerelor
        for (int i = 0; i < 8; i++) {
            routers[i] = new Router(gen_name("Router", i+1).c_str());
            routers[i]->cfg_port(cfg_ports[i]); // portul de config
        }

        // conectare lantul Est-Vest între routere
        for (int i = 0; i < 7; i++) {
            link_fwd[i] = new sc_fifo<packet>(16);
            link_bwd[i] = new sc_fifo<packet>(16);

            // R[i] ieșire Est -> R[i+1] intrare Vest
            routers[i]->out_ports[E](*link_fwd[i]);
            routers[i+1]->in_ports[V](*link_fwd[i]);

            // R[i+1] ieșire Vest -> R[i] intrare Est
            routers[i+1]->out_ports[V](*link_bwd[i]);
            routers[i]->in_ports[E](*link_bwd[i]);
        }

        // Conectare CPU la Router
        auto connect_cpu = [&](int r_idx, int port, int id, int target, int addr, int data) {
            CPU* c = new CPU(gen_name("CPU", id).c_str(), id, target, addr, data);
            cpus.push_back(c);

            // Firul 1: CPU -> Router (Request)
            sc_fifo<packet>* f_req = new sc_fifo<packet>(16);
            c->out_port(*f_req);
            routers[r_idx]->in_ports[port](*f_req);
            periph_fifos.push_back(f_req);

            // Firul 2: Router -> CPU (Response)
            sc_fifo<packet>* f_rsp = new sc_fifo<packet>(16);
            routers[r_idx]->out_ports[port](*f_rsp);
            c->in_port(*f_rsp);
            periph_fifos.push_back(f_rsp);
        };

        // Conectare Memorie la un router
        auto connect_mem = [&](int r_idx, int port, int id) {
            MEM* m = new MEM(gen_name("MEM", id).c_str(), id);
            mems.push_back(m);

            // Firul 1: Router -> MEM (Request)
            sc_fifo<packet>* f_req = new sc_fifo<packet>(16);
            routers[r_idx]->out_ports[port](*f_req);
            m->in_port(*f_req);
            periph_fifos.push_back(f_req);

            // Firul 2: MEM -> Router (Response)
            sc_fifo<packet>* f_rsp = new sc_fifo<packet>(16);
            m->out_port(*f_rsp);
            routers[r_idx]->in_ports[port](*f_rsp);
            periph_fifos.push_back(f_rsp);
        };

        // Închide un port (conectează la nimic/dummy)
        auto close_port = [&](int r_idx, int port) {
            sc_fifo<packet>* d1 = new sc_fifo<packet>(16);
            sc_fifo<packet>* d2 = new sc_fifo<packet>(16);
            routers[r_idx]->in_ports[port](*d1);
            routers[r_idx]->out_ports[port](*d2);
            periph_fifos.push_back(d1);
            periph_fifos.push_back(d2);
        };

        
        // IMPLEMENTARE TOPOLOGIE

        // ROUTER 1
        close_port(0, N);
        connect_mem(0, S, 83);
        connect_cpu(0, V, 20, 200, 10, 83); // Singurul CPU activ: 20 -> 200
        

        // ROUTER 2
        close_port(1, N); 
        close_port(1, S);

        // ROUTER 3
        close_port(2, N); 
        close_port(2, S); 

        // ROUTER 4
        close_port(3, N);
        close_port(3, S);
        //connect_mem(3, S, 100);

        // ROUTER 5
        close_port(4, N);
        close_port(4, S);

        // ROUTER 6
        close_port(5, N); 
        close_port(5, S); 

        // ROUTER 7
        close_port(6, N);
        //connect_cpu(6, N, 8, 83, 10, 15);
        close_port(6, S); 

        // ROUTER 8
        close_port(7, N); 
        //close_port(&, E);
        connect_mem(7, E, 200); 
        close_port(7, S);



        // // ROUTER 1 (Stânga)
        // close_port(0, N);
        // // E -> Legat de backbone
        // connect_mem(0, S, 83);
        // connect_cpu(0, V, 20, 200, 10, 83); // CPU 20 scrie 83 la MEM 200

        // // ROUTER 2
        // //connect_cpu(1, N, 14, 10, 5, 99); // CPU 14 scrie 99 la MEM 10
        // close_port(1, S);

        // // ROUTER 3
        // //connect_cpu(2, N, 60, 60, 2, 77); // CPU 60 scrie la MEM 60
        // //connect_cpu(2, S, 24, 32, 8, 44); // CPU 24 scrie la MEM 32

        // // ROUTER 4
        // close_port(3, N);
        // //connect_mem(3, S, 100);

        // // ROUTER 5
        // close_port(4, N);
        // close_port(4, S);

        // // ROUTER 6
        // //connect_mem(5, N, 10);
        // //connect_mem(5, S, 60);

        // // ROUTER 7
        // //connect_cpu(6, N, 8, 83, 10, 15); // CPU 8 scrie 15 la MEM 83
        // //connect_mem(6, S, 32);

        // // ROUTER 8 (Dreapta)
        // //connect_mem(7, N, 50);
        // close_port(7, N);
        // connect_mem(7, E, 200);
        // close_port(7, S);
    }
};


int sc_main(int argc, char* argv[]) {
    Network net("System");

    // Canale de configurare
    sc_fifo<cfg_trans> cfg_fifos[8];
    for(int i=0; i<8; i++) {
        net.cfg_ports[i](cfg_fifos[i]);
    }

    cout << "--- START L1 SIMULATION ---" << endl;

    
    // Configurare tabela de rutare pentru MEM 200 spre EST
    for(int i=0; i<7; i++) cfg_fifos[i].write(cfg_trans(cfg_trans::SET_ROUTE, 200, E));
    // Routerul 8 scoate 200 pe portul E
    cfg_fifos[7].write(cfg_trans(cfg_trans::SET_ROUTE, 200, E));

    // Configurare tabela de rutare pentru MEM 200 spre VEST
    for(int i=1; i<8; i++) cfg_fifos[i].write(cfg_trans(cfg_trans::SET_ROUTE, 20, V));
    cfg_fifos[0].write(cfg_trans(cfg_trans::SET_ROUTE, 20, V));

    //Configurare MEM 83
    cfg_fifos[0].write(cfg_trans(cfg_trans::SET_ROUTE, 83, S));
    for(int i=1; i<8; i++) cfg_fifos[i].write(cfg_trans(cfg_trans::SET_ROUTE, 83, V));

    // Configurare Răspuns pt CPU 8 pana la MEM 83
    for(int i=0; i<6; i++) cfg_fifos[i].write(cfg_trans(cfg_trans::SET_ROUTE, 8, E));
    cfg_fifos[6].write(cfg_trans(cfg_trans::SET_ROUTE, 8, N));

    // Configurare MEM 100
    for(int i=0; i<3; i++) cfg_fifos[i].write(cfg_trans(cfg_trans::SET_ROUTE, 100, E));
    cfg_fifos[3].write(cfg_trans(cfg_trans::SET_ROUTE, 100, S));
    
    sc_start(1000, SC_NS); 

    cout << "--- END L1 SIMULATION ---" << endl;
    return 0;
}