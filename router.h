#ifndef ROUTER_H
#define ROUTER_H

#include <systemc.h>
#include "utils.h"
#include <map>
#include <string>

SC_MODULE(Router) {
    // Communication ports (N=0, S=1, E=2, V=3)
    sc_fifo_in<packet>   in_ports[4];   // Packet input ports
    sc_fifo_out<packet>  out_ports[4];  // Packet output ports
    sc_fifo_in<cfg_trans> cfg_port;     // Configuration port

    std::map<int, std::vector<int>> routing_table;   // Routing table: maps Destination ID to Output Port ID
    bool port_enabled[4];               // Port status: true = active, false = disabled

    int arbitration_policy; // 0 = Fixed Priority, 1 = Round Robin
    int last_served_port;   // Stores the last served port ID for Round Robin logic
    int max_q_len = 16;     // Configurable queue length limit

    int routed_packets = 0;     // Total successfully forwarded packets
    int dropped_ttl = 0;        // Packets dropped due to TTL expiration
    int dropped_no_route = 0;   // Packets dropped because no route was found
    int dropped_disabled = 0;   // Packets dropped because the target port was disabled
    int rerouted_count = 0;     // Counter for when an alternative route was chosen
    
    void process() {
        while (true) {
            wait(10, SC_NS); // Processing latency per cycle

            // 1. CHECK CONFIGURATION
            cfg_trans cfg;
            while (cfg_port.nb_read(cfg)) { // Non-blocking read for configuration commands
                handle_config(cfg);
            }

            // 2. ARBITRATION AND PORT SELECTION
            int start_idx = 0;
            if (arbitration_policy == PRIORITY) {
                start_idx = 0; // FIXED PRIORITY: always start checking from port 0 (North)
            } else {
                // ROUND ROBIN: start from the next port after the last served one
                start_idx = (last_served_port + 1) % 4; 
            }

            // Iterate through ports starting from start_idx
            for (int i = 0; i < 4; i++) { 
                int current_port = (start_idx + i) % 4;

                // Skip disabled ports
                if (!port_enabled[current_port]) continue;

                packet p;
                if (in_ports[current_port].nb_read(p)) {
                    cout << "@" << sc_time_stamp() << " [ROUTER " << name() << "] Pkt in port " 
                         << PortNames[current_port] << ": " << p << endl;

                    // TTL LOGIC
                    if (p.ttl <= 0) {
                        dropped_ttl++;
                        cout << "    -> DROP: TTL Expired" << endl;
                        continue;
                    }
                    p.ttl--; 
                    
                    // ROUTING LOGIC
                    if (routing_table.find(p.dst_id) != routing_table.end()) {
                        std::vector<int>& options = routing_table[p.dst_id];
                        int best_out_port = -1;

                        if (options.size() == 1) {
                            best_out_port = options[0]; // Backward compatible
                        } else {
                            // Adaptive selection: Find port with most free space
                            int max_free = -1;
                            for (int port_id : options) {
                                if (port_enabled[port_id]) {
                                    int current_free = out_ports[port_id]->num_free();
                                    if (current_free > max_free) {
                                        max_free = current_free;
                                        best_out_port = port_id;
                                    }
                                }
                            }
                            if (best_out_port != -1) rerouted_count++; 
                        }
                        
                        // Final forwarding check
                        if (best_out_port != -1 && port_enabled[best_out_port]) {
                             out_ports[best_out_port].write(p);
                             routed_packets++;
                             cout << "    -> Fwd to Port " << PortNames[best_out_port] << endl;
                        } else {
                            dropped_disabled++;
                            cout << "    -> DROP: Output Port(s) disabled or unavailable" << endl;
                        }
                    } else {
                        dropped_no_route++;
                        cout << "    -> DROP: No route for Destination " << p.dst_id << endl;
                    }

                    // Update last served port for Round Robin
                    last_served_port = current_port;
                    break; // Serve one packet per cycle
                }
            }
        }
    }

    // Process configuration commands
    void handle_config(cfg_trans c) {
        switch (c.type) {
            case cfg_trans::SET_ROUTE:
                // If the route already exists, we add it as an alternative
                // To maintain L1/L2 logic, use a unique check
                if (std::find(routing_table[c.target].begin(), 
                              routing_table[c.target].end(), 
                              c.value) == routing_table[c.target].end()) {
                    routing_table[c.target].push_back(c.value);
                }
                cout << "@" << sc_time_stamp() << " [CFG " << name() << "] Added Route: Dst " 
                     << c.target << " -> Port " << PortNames[c.value] << endl;
                break;
            case cfg_trans::ENABLE_PORT:
                port_enabled[c.target] = (c.value != 0);
                cout << "@" << sc_time_stamp() << " [CFG " << name() << "] Port " 
                     << c.target << (c.value ? " ENABLED" : " DISABLED") << endl;
                break;
            case cfg_trans::SET_ARBITER:
                arbitration_policy = c.value;
                cout << "@" << sc_time_stamp() << " [CFG " << name() << "] Arbiter: " 
                     << (c.value ? "Round-Robin" : "Fixed Priority") << endl;
                break;
            case cfg_trans::SET_Q_LEN:
                max_q_len = c.value;
                cout << "@" << sc_time_stamp() << " [CFG " << name() << "] Max Queue Length: " << max_q_len << endl;
                break;
        }
    }

    // Display performance statistics
    void print_stats() {
        cout << "-------------------------------------------" << endl;
        cout << " Statistics for Router: " << name() << endl;
        cout << "  - Packets Routed Successfully: " << routed_packets << endl;
        cout << "  - Alternative Routes Used:     " << rerouted_count << endl;
        cout << "  - Packets Dropped (TTL):       " << dropped_ttl << endl;
        cout << "  - Packets Dropped (No Route):  " << dropped_no_route << endl;
        cout << "  - Packets Dropped (Disabled):  " << dropped_disabled << endl;
        cout << "-------------------------------------------" << endl;
    }

    SC_CTOR(Router) {
        SC_THREAD(process);
        for(int i=0; i<4; i++) port_enabled[i] = true;
        
        // Default initializations
        arbitration_policy = PRIORITY; 
        last_served_port = 3; 
    }
};

#endif