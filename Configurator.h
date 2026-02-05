#ifndef CONFIGURATOR_H
#define CONFIGURATOR_H

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include "json.hpp"
#include "utils.h"

using json = nlohmann::json;
using namespace std;

// Structures for system configuration
struct LinkDef { int src_r, dst_r, src_p, dst_p; };

struct CpuTaskDef {
    string op; 
    int target, addr, data, delay;
};

struct DeviceDef {
    string type; 
    int id, router_id, port_id;
    vector<CpuTaskDef> tasks; // List of CPU instructions
};

// L3 Update: Route definition now supports multiple output ports
struct RouteDef { 
    int target; 
    int router; 
    vector<int> out_ports; 
};

struct RouterCfg { int id, q_len, arb; };

class Configurator {
public:
    int num_routers;
    vector<LinkDef> links;
    vector<DeviceDef> devices;
    vector<RouteDef> routes;
    vector<RouterCfg> router_settings;

    // Helper to map string direction to PortID enum
    int getPortId(string p) {
        if (p == "N") return 0; if (p == "S") return 1;
        if (p == "E") return 2; if (p == "V") return 3;
        return -1;
    }

    // Load and parse the JSON configuration file
    bool load(string filename) {
        ifstream f(filename);
        if (!f.is_open()) return false;
        
        try {
            json j = json::parse(f);
            num_routers = j["system"]["num_routers"];

            // 1. Parse Network Links
            for (auto& l : j["links"]) {
                links.push_back({l["src_r"], l["dst_r"], getPortId(l["src_p"]), getPortId(l["dst_p"])});
            }

            // 2. Parse Devices & CPU Instruction Tasks
            for (auto& d : j["devices"]) {
                DeviceDef dd;
                dd.type = d["type"];
                dd.id = d["id"];
                dd.router_id = d["router"];
                dd.port_id = getPortId(d["port"]);

                if (dd.type == "CPU" && d.contains("tasks")) {
                    for (auto& t : d["tasks"]) {
                        CpuTaskDef task;
                        task.op = t["op"];
                        task.target = t["target"];
                        task.addr = t["addr"];
                        task.data = t.value("data", 0);
                        task.delay = t.value("delay", 0);
                        dd.tasks.push_back(task);
                    }
                }
                devices.push_back(dd);
            }

            // 3. Parse Routing Tables with Multi-path support
            for (auto& r : j["routing_table"]) {
                RouteDef rd;
                rd.target = r["target"];
                rd.router = r["router"];
                
                // If "out" is an array, add all ports; otherwise add the single port
                if (r["out"].is_array()) {
                    for (auto& p_str : r["out"]) {
                        rd.out_ports.push_back(getPortId(p_str));
                    }
                } else {
                    rd.out_ports.push_back(getPortId(r["out"]));
                }
                routes.push_back(rd);
            }
            
            // 4. Parse Individual Router Settings (Queue Length, Arbiter)
            if(j.contains("routers_config")) {
                for(auto& rc : j["routers_config"]) {
                    router_settings.push_back({rc["id"], rc["queue_limit"], rc["arbiter"]});
                }
            }

        } catch (exception& e) {
            cerr << "JSON Parsing Error: " << e.what() << endl; 
            return false;
        }
        return true;
    }
};
#endif