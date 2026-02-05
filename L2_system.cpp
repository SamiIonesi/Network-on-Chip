#include <systemc.h>
#include <vector>
#include "utils.h"
#include "router.h"
#include "cpu_v2.h" // Noul CPU
#include "mem.h"
#include "Configurator.h"

using namespace std;

std::string gen_name(const char* prefix, int id) {
    return string(prefix) + "_" + to_string(id);
}

int sc_main(int argc, char* argv[]) {
    Configurator cfg;
    if (!cfg.load("config.json")) {
        cerr << "Error loading config.json" << endl;
        return -1;
    }

    // --- Componente ---
    vector<Router*> routers;
    vector<CPU_L2*> cpus;
    vector<MEM*> mems;
    vector<sc_fifo<packet>*> fifos; 
    vector<sc_fifo<cfg_trans>*> cfg_busses; // Bus-urile de configurare

    // 1. Instantiere Routere
    for(int i=0; i<cfg.num_routers; i++) {
        Router* r = new Router(gen_name("Router", i).c_str());
        sc_fifo<cfg_trans>* cf = new sc_fifo<cfg_trans>(16);
        r->cfg_port(*cf);
        
        routers.push_back(r);
        cfg_busses.push_back(cf);
    }

    // 2. Conectare Link-uri (Mesh Topology din JSON)
    for(auto& l : cfg.links) {
        sc_fifo<packet>* f_fwd = new sc_fifo<packet>(16);
        fifos.push_back(f_fwd);
        // Link Simplu (Unidirectional definit in JSON, daca vrei bidirectional trebuiesc 2 linii in JSON)
        routers[l.src_r]->out_ports[l.src_p](*f_fwd);
        routers[l.dst_r]->in_ports[l.dst_p](*f_fwd);
    }

    // 3. Conectare Device-uri si Configurare CPU
    for(auto& d : cfg.devices) {
        sc_fifo<packet>* to_dev = new sc_fifo<packet>(16);
        sc_fifo<packet>* from_dev = new sc_fifo<packet>(16);
        fifos.push_back(to_dev); fifos.push_back(from_dev);

        if (d.type == "CPU") {
            CPU_L2* cpu = new CPU_L2(gen_name("CPU", d.id).c_str(), d.id);
            cpu->in_port(*to_dev);
            cpu->out_port(*from_dev);
            
            // Incarcam Programul in CPU (Tranzactiile)
            for(auto& t : d.tasks) {
                int type = (t.op == "WRITE") ? packet::REQ_WRITE : packet::REQ_READ;
                cpu->add_task(type, t.target, t.addr, t.data, t.delay);
            }
            cpus.push_back(cpu);
        } else {
            MEM* mem = new MEM(gen_name("MEM", d.id).c_str(), d.id);
            mem->in_port(*to_dev);
            mem->out_port(*from_dev);
            mems.push_back(mem);
        }

        // Legam la Router
        routers[d.router_id]->out_ports[d.port_id](*to_dev);
        routers[d.router_id]->in_ports[d.port_id](*from_dev);
        
        // Activam portul
        cfg_busses[d.router_id]->write(cfg_trans(cfg_trans::ENABLE_PORT, d.port_id, 1));
    }

    // 4. Configurare Routere (Parametri si Rute)
    cout << "[SYS] Configuring Routers..." << endl;
    
    // Setare Cozi si Arbitri
    for(auto& rs : cfg.router_settings) {
        cfg_busses[rs.id]->write(cfg_trans(cfg_trans::SET_Q_LEN, 0, rs.q_len)); // Target=0 pt QLEN
        cfg_busses[rs.id]->write(cfg_trans(cfg_trans::SET_ARBITER, 0, rs.arb));
    }

    // Setare Tabela de Rutare
    for(auto& r : cfg.routes) {
        cfg_busses[r.router]->write(cfg_trans(cfg_trans::SET_ROUTE, r.target, r.out_ports));
    }
    
    // 5. Inchidere Porturi Neutilizate (Anti-Eroare Port Not Bound)
    for(int i=0; i<cfg.num_routers; i++) {
        for(int p=0; p<4; p++) {
            if(!routers[i]->in_ports[p].get_interface()) {
                sc_fifo<packet>* d = new sc_fifo<packet>(1);
                routers[i]->in_ports[p](*d); fifos.push_back(d);
            }
            if(!routers[i]->out_ports[p].get_interface()) {
                sc_fifo<packet>* d = new sc_fifo<packet>(1);
                routers[i]->out_ports[p](*d); fifos.push_back(d);
            }
        }
    }

    cout << "--- START MESH SIMULATION ---" << endl;
    sc_start(5000, SC_NS);
    return 0;
}