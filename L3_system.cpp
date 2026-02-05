#include <systemc.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include "utils.h"
#include "router.h"
#include "cpu_v2.h" // Ensure this matches your filename (e.g., cpu_l2.h or cpu_v2.h)
#include "mem.h"
#include "Configurator.h"

using namespace std;

int sc_main(int argc, char* argv[]) {
    // 1. Load Configuration
    Configurator cfg;
    if (!cfg.load("config_L3.json")) {
        cerr << "[ERROR] Cannot load config_L3.json" << endl;
        return -1;
    }

    // Containers for system modules
    vector<Router*> routers;
    vector<CPU_L2*> cpus;
    vector<MEM*> mems;
    vector<sc_fifo<packet>*> fifos;
    vector<sc_fifo<cfg_trans>*> cfg_busses;

    // 2. Instantiate Routers
    for(int i=0; i<cfg.num_routers; i++) {
        // Generate unique name for each router
        string r_name = "Router_" + to_string(i);
        Router* r = new Router(r_name.c_str());
        
        sc_fifo<cfg_trans>* cb = new sc_fifo<cfg_trans>(20);
        r->cfg_port(*cb);
        
        routers.push_back(r);
        cfg_busses.push_back(cb);
    }

    // 3. Connect Network Links
    for(auto& l : cfg.links) {
        sc_fifo<packet>* f = new sc_fifo<packet>(16);
        routers[l.src_r]->out_ports[l.src_p](*f);
        routers[l.dst_r]->in_ports[l.dst_p](*f);
        fifos.push_back(f);
    }

    // 4. Connect Devices (CPUs & MEMs)
    for(auto& d : cfg.devices) {
        sc_fifo<packet>* q1 = new sc_fifo<packet>(16); // Router -> Device
        sc_fifo<packet>* q2 = new sc_fifo<packet>(16); // Device -> Router

        if (d.type == "CPU") {
            // FIX for W505: Unique name for CPU
            string cpu_name = "CPU_" + to_string(d.id);
            CPU_L2* c = new CPU_L2(cpu_name.c_str(), d.id);
            
            c->out_port(*q2); // Device sends to Router
            c->in_port(*q1);  // Device receives from Router
            
            for(auto& t : d.tasks) {
                c->add_task(t.op == "WRITE" ? packet::REQ_WRITE : packet::REQ_READ, 
                           t.target, t.addr, t.data, t.delay);
            }
            cpus.push_back(c);
        } else {
            // FIX for W505: Unique name for MEM
            string mem_name = "MEM_" + to_string(d.id);
            MEM* m = new MEM(mem_name.c_str(), d.id);
            
            m->out_port(*q2);
            m->in_port(*q1);
            mems.push_back(m);
        }

        // Link Device FIFOs to Router Ports
        routers[d.router_id]->out_ports[d.port_id](*q1);
        routers[d.router_id]->in_ports[d.port_id](*q2);
        
        fifos.push_back(q1); 
        fifos.push_back(q2);
    }

    // 5. FIX E109: Bind ALL remaining unbound ports to dummy FIFOs
    // In a 36-router Torus, many N/S/E/V ports might still be null if not linked
    for(auto r : routers) {
        for(int p=0; p<4; p++) {
            // Check Input Port
            if (!r->in_ports[p].get_interface()) { 
                sc_fifo<packet>* dummy_in = new sc_fifo<packet>(1); 
                r->in_ports[p](*dummy_in); 
                fifos.push_back(dummy_in);
            }
            // Check Output Port
            if (!r->out_ports[p].get_interface()) { 
                sc_fifo<packet>* dummy_out = new sc_fifo<packet>(1); 
                r->out_ports[p](*dummy_out); 
                fifos.push_back(dummy_out);
            }
        }
    }

    // 6. Configure Routes (Support for L3 Multipath)
    cout << "[SYS] Configuring L3 Routing Tables..." << endl;
    for(auto& rt : cfg.routes) {
        for(int p : rt.out_ports) {
            cfg_busses[rt.router]->write(cfg_trans(cfg_trans::SET_ROUTE, rt.target, p));
        }
    }

    // 7. Start Simulation
    cout << "--- STARTING TORUS 6x6 L3 SIMULATION ---" << endl;
    sc_start(5000, SC_NS);
    cout << "--- SIMULATION FINISHED ---" << endl;

    // 8. Generate Performance Report CSV
    ofstream csv("performance_report.csv");
    csv << "Entity,Routed,TTLDrops,Reroutes,AvgLatency_ns" << endl;

    for(auto r : routers) {
        csv << r->name() << "," << r->routed_packets << "," 
            << r->dropped_ttl << "," << r->rerouted_count << ",0" << endl;
        r->print_stats();
    }

    for(auto c : cpus) {
        double lat = (c->received_packets > 0) ? (c->total_latency.to_double() / c->received_packets) : 0;
        csv << "CPU_" << c->my_id << ",0,0,0," << lat << endl;
        c->print_cpu_stats();
    }
    
    csv.close();
    cout << "[SYS] Report saved to performance_report.csv" << endl;

    return 0;
}