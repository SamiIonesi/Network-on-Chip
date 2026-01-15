// router.h
#ifndef ROUTER_H
#define ROUTER_H

#include <systemc.h>
#include "utils.h"
#include <map>

SC_MODULE(Router) {
    //Deci practic acestea sunt porturile de intrare/iesire ale routerului (sau drumurile in analogia cu traficul rutier)
    sc_fifo_in<packet>  in_ports[4]; // 0=N, 1=S, 2=E, 3=V, intrarile de pachete
    sc_fifo_out<packet> out_ports[4]; // iesirile de pachete
    sc_fifo_in<cfg_trans> cfg_port; // portul de configurare

    std::map<int, int> routing_table; // tabela de rutare: asociaza destinatii cu porturi de iesire (ex: Dst 10 -> Port 0)
    bool port_enabled[4]; // statusul porturilor: true = activat, false = dezactivat

    int arbitration_policy; // 0 = Prioritate Fixa, 1 = Round Robin
    int last_served_port;   // Tine minte ultimul port servit (pentru Round Robin)

    void process() {
        while (true) {
            wait(10, SC_NS); //Routerul practic nu e instantaneu. Îi ia 10 nanosecunde să proceseze un pachet.

            // 1. VERIFICĂ CONFIGURAREA (Deci practic inainte de a procesa pachete, ne uitam daca avem noi comenzi de configurare)
            cfg_trans cfg;
            while (cfg_port.nb_read(cfg)) { //non-blocking read (daca nu e nimic de citit, trece mai departe)
                handle_config(cfg); //caz in care ne ducem sa procesam comanda de configurare
            }

            // 2. ARBITRARE SI SELECTIE PORT
            // Aici decidem de la care port incepem sa verificam
            int start_idx = 0;

            if (arbitration_policy == PRIORITY) {
                start_idx = 0; // In mod PRIORITATE, incepem mereu de la 0 (Nord)
            } else {
                // In mod ROUND ROBIN, incepem de la urmatorul dupa cel servit ultima data
                start_idx = (last_served_port + 1) % 4; 
            }

            // Iteram prin porturi incepand de la start_idx
            for (int i = 0; i < 4; i++) { 
                
                // Calculam portul curent folosind modulo (%) pentru a ne invarti in cerc (ex: 3 -> 0)
                int current_port = (start_idx + i) % 4;

                // Dacă portul e dezactivat, îl sărim
                if (!port_enabled[current_port]) continue;

                packet p;
                if (in_ports[current_port].nb_read(p)) {
                    cout << "@" << sc_time_stamp() << " [ROUTER] Pkt in port " << PortNames[current_port] << ": " << p;
                    
                    // Rutare
                    if (routing_table.find(p.dst_id) != routing_table.end()) {
                        int out_idx = routing_table[p.dst_id];
                        
                        if (port_enabled[out_idx]) {
                             out_ports[out_idx].write(p);
                             cout << " -> Fwd to Port " << PortNames[out_idx] << endl;
                        } else {

                            cout << " -> DROP: Port " << PortNames[out_idx] << " disabled" << endl;
                        }
                    } else {
                        cout << " -> DROP: No route for Destination " << p.dst_id << endl;
                    }

                    // Actualizam ultimul port servit (imp pt Round Robin)
                    last_served_port = current_port;

                    break; 
                }
            }
        }
    }

    void handle_config(cfg_trans c) {
        switch (c.type) {
            case cfg_trans::SET_ROUTE:
                routing_table[c.target] = c.value;
                cout << "@" << sc_time_stamp() << " [CFG] Route: Dst " << c.target << "->Port " << PortNames[c.value] << endl;
                break;
            case cfg_trans::ENABLE_PORT:
                port_enabled[c.target] = (c.value != 0);
                cout << "@" << sc_time_stamp() << " [CFG] Port " << c.target << (c.value ? " ON" : " OFF") << endl;
                break;
            case cfg_trans::SET_ARBITER:
                arbitration_policy = c.value;
                cout << "@" << sc_time_stamp() << " [CFG] Arbiter changed to: " << (c.value ? "Round-Robin" : "Fixed Priority") << endl;
                break;
        }
    }

    SC_CTOR(Router) {
        SC_THREAD(process);
        for(int i=0; i<4; i++) port_enabled[i] = true;
        
        // Initializari default
        arbitration_policy = PRIORITY; // Pornim implicit cu Prioritate Fixa
        last_served_port = 3; // Ca sa incepem cu 0 prima data daca trecem pe RR
    }
};

#endif