#ifndef MEM_H
#define MEM_H

#include <systemc.h>
#include <map>
#include "utils.h"

SC_MODULE(MEM) {
    sc_fifo_in<packet>  in_port;  // Intrare: Primește Cereri (REQ_WRITE / REQ_READ)
    sc_fifo_out<packet> out_port; // Ieșire: Trimite Răspunsuri (RSP_ACK / RSP_DATA)
    
    int my_id; //adresa memoriei in retea

    // Aici practic vom stoca datele (addr -> data)
    std::map<int, int> memory_space; 

    void behavior() {
        while(true) {

            packet req = in_port.read(); // deci practic memoria sta inactiva si asteapta cereri
            
            packet rsp; 
            bool send_response = false; 

            // afisam ce am primit de la CPU
            cout << "@" << sc_time_stamp() << " [MEM " << my_id << "] RECV: " << req << endl;

            // Procesam raspunsul in functie de tipul cererii
            switch(req.type) {
                // --- Write ---
                case packet::REQ_WRITE:
                    memory_space[req.address] = req.data; // scriem in memorie
                    
                    cout << "      ---> [WRITE OP] Written value " << req.data 
                         << " at address " << req.address << endl;
                    
                    // Construim confirmarea (ACK)
                    rsp.type = packet::RSP_ACK;
                    rsp.data = 0; // Nu contează la ACK
                    send_response = true;
                    break;

                // --- Read ---
                case packet::REQ_READ:
                    int found_value;
                    
                    // verificam daca adresa exista in memorie
                    if (memory_space.find(req.address) != memory_space.end()) {
                        found_value = memory_space[req.address];
                    } else {
                        // Dacă adresa nu a fost scrisă niciodată, returnăm 0 (sau o eroare)
                        found_value = 0;
                        cout << "      ---> [READ OP] Address empty. Returning 0." << endl;
                    }

                    cout << "      ---> [READ OP] Read value " << found_value 
                         << " from address " << req.address << endl;

                    // Construim pachetul de date
                    rsp.type = packet::RSP_DATA;
                    rsp.data = found_value;
                    send_response = true;
                    break;

                default:
                    // Ignorăm pachete de tip ACK/DATA dacă ajung din greșeală aici
                    cout << "      ---> [IGNORED] Unexpected packet type." << endl;
                    break;
            }

            // trimitem raspunsul inapoi la CPU
            if (send_response) {
                // Inversăm rolurile: MEM devin Sursa, CPU-ul devine Destinatia
                rsp.src_id = my_id;       
                rsp.dst_id = req.src_id;  
                rsp.address = req.address;

                wait(10, SC_NS);
                
                out_port.write(rsp);
                cout << "      ---> [REPLY] Sending response to CPU " << rsp.dst_id << endl;
            }
        }
    }

    SC_HAS_PROCESS(MEM);

    MEM(sc_module_name name, int id) : sc_module(name), my_id(id) {
        SC_THREAD(behavior);
    }
};

#endif