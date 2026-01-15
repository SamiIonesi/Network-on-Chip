#include <systemc.h>
#include <iostream>
#include <map>
#include <vector>
#include <iomanip>

using namespace std;

enum PortID { N = 0, S = 1, E = 2, V = 3 }; 
// Definim modurile: 0 = Prioritate Fixa (mereu incepe cu N), 1 = Round Robin (pe rand)
enum ArbMode { PRIORITY = 0, ROUND_ROBIN = 1 }; 

// Asta este practic "masina" care transporta datele
struct packet {
    int src_id; //Adresa expeditor (CPU)
    int dst_id; //Adresa destinatar (MEM)
    int data;   //Date utile (marfa transportata)
    
    // Constructor default (necesar pentru sc_fifo pt a crea pachete goale)
    packet() : src_id(0), dst_id(0), data(0) {}

    // Constructor cu parametri (pt a crea pachete cu date)
    packet(int s, int d, int val) : src_id(s), dst_id(d), data(val) {}

    // Operator == (necesar pentru sc_signal)
    bool operator==(const packet& other) const {
        return (src_id == other.src_id && dst_id == other.dst_id && data == other.data);
    }
    
    // Operator << (necesar pentru afisare si sc_fifo dump)
    friend ostream& operator<<(ostream& os, const packet& p) {
        os << "[Src:" << p.src_id << " -> Dst:" << p.dst_id << "   Data:" << p.data << "]";
        return os;
    }
};

// Structura pentru tranzactii de configurare (deci practic cu acesta ii spunem routerului ce sa faca)
struct cfg_trans {
    // AM ADAUGAT INAPOI SET_ARBITER
    enum Type { SET_ROUTE = 0, ENABLE_PORT = 1, SET_Q_LEN = 2, SET_ARBITER = 3 };
    // SET_ROUTE: comanda de schimbare a tabelei de rutare
    // ENABLE_PORT: comanda de activare/dezactivare port
    // SET_Q_LEN: comanda de setare lungime coada
    // SET_ARBITER: comanda de schimbare a regulii de prioritate

    int type; //Tipul comenzii
    int target; //Pt SET_ROUTE: adresa destinatar; Pt ENABLE_PORT: id port
    int value;  //Pt SET_ROUTE: id port de iesire; Pt SET_ARBITER: 0=FixPriority, 1=RR

    cfg_trans() : type(0), target(0), value(0) {}

    cfg_trans(int t, int tg, int v) : type(t), target(tg), value(v) {}

    bool operator==(const cfg_trans& other) const {
        return (type == other.type && target == other.target && value == other.value);
    }

    friend ostream& operator<<(ostream& os, const cfg_trans& t) {
        os << "{CFG Type:" << t.type << " Tgt:" << t.target << " Val:" << t.value << "}";
        return os;
    }
};


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
                    cout << "@" << sc_time_stamp() << " [ROUTER] Pkt in port " << current_port << ": " << p;
                    
                    // Rutare
                    if (routing_table.find(p.dst_id) != routing_table.end()) {
                        int out_idx = routing_table[p.dst_id];
                        
                        if (port_enabled[out_idx]) {
                             out_ports[out_idx].write(p);
                             cout << " -> Fwd to Port " << out_idx << endl;
                        } else {
                            cout << " -> DROP: Port " << out_idx << " disabled" << endl;
                        }
                    } else {
                        cout << " -> DROP: No route for Dest " << p.dst_id << endl;
                    }

                    // Actualizam ultimul port servit (important pentru Round Robin)
                    last_served_port = current_port;

                    // Ne oprim după ce am servit UN pachet.
                    break; 
                }
            }
        }
    }

    void handle_config(cfg_trans c) {
        switch (c.type) {
            case cfg_trans::SET_ROUTE:
                routing_table[c.target] = c.value;
                cout << "@" << sc_time_stamp() << " [CFG] Route: Dst " << c.target << "->Port " << c.value << endl;
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


int sc_main(int argc, char* argv[]) {
    Router r1("Router1");

    sc_fifo<packet> fifo_in[4];
    sc_fifo<packet> fifo_out[4];
    sc_fifo<cfg_trans> fifo_cfg;

    // Conectare
    r1.cfg_port(fifo_cfg);
    for (int i = 0; i < 4; i++) {
        r1.in_ports[i](fifo_in[i]);
        r1.out_ports[i](fifo_out[i]);
    }

    cout << "--- START L0 ---" << endl;

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
    fifo_in[V].write(packet(99, 10, 100));

    cout << "Injecting packet 2 (on Port 2)..." << endl;
    fifo_in[E].write(packet(20, 34, 100)); 
    
    cout << "Injecting packet 3 (on Port 0 - HIGH PRIORITY)..." << endl;
    fifo_in[N].write(packet(88, 20, 500));

    cout << "Injecting packet 4 (on Port 1)..." << endl;
    fifo_in[S].write(packet(120, 35, 40));

    cout << "Injecting packet 5 (on Port 1)..." << endl;
    fifo_in[S].write(packet(1, 120, 43));


    sc_start(100, SC_NS); 

    // 3. CHECK
    packet p_out;
    
    // Verificăm Nord (Port 0)
    if (fifo_out[N].nb_read(p_out)) {
        cout << "Received on NORTH (Port 0): " << p_out << endl;
    } else {
        cout << "NORTH (Port 0) is empty." << endl;
    }

    // Verificăm Sud (Port 1)
    if (fifo_out[S].nb_read(p_out)) {
        cout << "Received on SOUTH (Port 1): " << p_out << endl;
    } else {
        cout << "SOUTH (Port 1) is empty." << endl;
    }

    // Verificăm Est (Port 2)
    if (fifo_out[E].nb_read(p_out)) {
        cout << "Received on EAST (Port 2): " << p_out << endl;
    } else {
        cout << "EAST (Port 2) is empty." << endl;
    }

    // Verificăm Vest (Port 3)
    if (fifo_out[V].nb_read(p_out)) {
        cout << "Received on WEST (Port 3): " << p_out << endl;
    } else {
        cout << "WEST (Port 3) is empty." << endl;
    }
    cout << "--- END ---" << endl;
    return 0;
}