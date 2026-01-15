#ifndef CPU_H
#define CPU_H

#include <systemc.h>
#include "utils.h"

SC_MODULE(CPU) {
    sc_fifo_out<packet> out_port; // Ieșire: Trimite Cereri (REQ_WRITE / REQ_READ)
    sc_fifo_in<packet>  in_port;  // Intrare: Primește Răspunsuri (RSP_ACK / RSP_DATA)

    int my_id;       // Identificatorul unic al acestui CPU
    int target_id;   // ID-ul Memoriei cu care va comunica
    int test_addr;   // Adresa de memorie pe care o va testa
    int test_data;   // Datele pe care le va scrie


    void behavior() {
        wait(20, SC_NS); // astept ca sa se faca configuratiile in reteaua

        // --- Write ---
        cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] INIT WRITE -> MEM " << target_id 
             << " | Adr:" << test_addr << " Val:" << test_data << endl;

        // pachetul de cerere
        packet p_req_wr(packet::REQ_WRITE, my_id, target_id, test_addr, test_data);
        out_port.write(p_req_wr);

        // blochez CPU si astept ACK de la MEM
        packet p_rsp;
        in_port.read(p_rsp); // Blocant

        if (p_rsp.type == packet::RSP_ACK) {
            cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] DONE WRITE (ACK Received)" << endl;
        } else {
            cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] ERROR: Expected ACK, got " << p_rsp << endl;
        }

        // facem pauza intre tranzactii
        wait(50, SC_NS);

        // --- Read ---
        // Verificam daca datele au fost scrise corect
        
        cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] INIT READ  -> MEM " << target_id 
             << " | Adr:" << test_addr << endl;

        // La citire, datele trimise sunt 0 (irelevante), contează doar adresa
        packet p_req_rd(packet::REQ_READ, my_id, target_id, test_addr, 0);
        out_port.write(p_req_rd);

        // Așteptăm datele înapoi
        in_port.read(p_rsp); // Blocant: Așteaptă DATA

        if (p_rsp.type == packet::RSP_DATA) {
            cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] DONE READ (Data Received): " 
                 << p_rsp.data << endl;
                 
            // Verificarea datei
            if (p_rsp.data == test_data) {
                cout << "      ---> SUCCESS: Read value matches written value!" << endl;
            } else {
                cout << "      ---> FAILURE: Data Mismatch!" << endl;
            }
        } else {
            cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] ERROR: Expected DATA, got " << p_rsp << endl;
        }
    }

    SC_HAS_PROCESS(CPU);

    CPU(sc_module_name name, int id, int target, int addr, int data) 
        : sc_module(name), my_id(id), target_id(target), test_addr(addr), test_data(data) 
    {
        SC_THREAD(behavior);
    }
};

#endif