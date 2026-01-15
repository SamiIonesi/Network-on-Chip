// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <systemc.h>
#include <iostream>

enum PortID { N = 0, S = 1, E = 2, V = 3 }; 
enum ArbMode { PRIORITY = 0, ROUND_ROBIN = 1 };
const char* PortNames[] = { "NORD", "SUD", "EST", "VEST" };

// Asta este practic "masina" care transporta datele -> L0
// struct packet {
//     int src_id; //Adresa expeditor (CPU)
//     int dst_id; //Adresa destinatar (MEM)
//     int data;   //Date utile (marfa transportata)
    
//     // Constructor default (necesar pentru sc_fifo pt a crea pachete goale)
//     packet() : src_id(0), dst_id(0), data(0) {}

//     // Constructor cu parametri (pt a crea pachete cu date)
//     packet(int s, int d, int val) : src_id(s), dst_id(d), data(val) {}

//     // Operator == (necesar pentru sc_signal)
//     bool operator==(const packet& other) const {
//         return (src_id == other.src_id && dst_id == other.dst_id && data == other.data);
//     }
    
//     // Operator << (necesar pentru afisare si sc_fifo dump)
//     friend ostream& operator<<(ostream& os, const packet& p) {
//         os << "[Src:" << p.src_id << " -> Dst:" << p.dst_id << "   Data:" << p.data << "]";
//         return os;
//     }
// };


struct packet {
    enum Type { 
        REQ_WRITE = 0, // CPU cere să scrie date în MEM
        REQ_READ = 1,  // CPU cere să citească date din MEM
        RSP_ACK = 2,   // MEM confirmă că a scris datele
        RSP_DATA = 3   // MEM trimite datele cerute înapoi la CPU
    };

    Type type;     // Tipul mesajului curent
    int src_id;    // Cine a inițiat (ex: CPU ID)
    int dst_id;    // Destinația curentă (ex: MEM ID)
    int address;   // Adresa din memorie unde scriem/citim
    int data;      // Datele efective (pentru WRITE sau RSP_DATA)

    // Constructor Default
    packet() : type(REQ_WRITE), src_id(0), dst_id(0), address(0), data(0) {}

    // Constructor Parametrizat
    packet(Type t, int s, int d, int addr, int val) 
        : type(t), src_id(s), dst_id(d), address(addr), data(val) {}

    // Operator == (Necesar pentru systemc semnale/fifo)
    bool operator==(const packet& other) const {
        return (type == other.type && src_id == other.src_id && 
                dst_id == other.dst_id && address == other.address && 
                data == other.data);
    }
    
    friend std::ostream& operator<<(std::ostream& os, const packet& p) {
        os << "[";
        switch(p.type) {
            case REQ_WRITE: os << "WRITE"; break;
            case REQ_READ:  os << "READ "; break;
            case RSP_ACK:   os << "ACK  "; break;
            case RSP_DATA:  os << "DATA "; break;
            default:        os << "???? "; break;
        }
        os << " Src:" << p.src_id << " -> Dst:" << p.dst_id 
           << " Addr:" << p.address << " Data:" << p.data << "]";
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


#endif