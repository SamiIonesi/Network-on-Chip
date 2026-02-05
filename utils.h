#ifndef UTILS_H
#define UTILS_H

#include <systemc.h>
#include <iostream>

// Port identifiers for the router
enum PortID { N = 0, S = 1, E = 2, V = 3 }; 
// Arbitration policies
enum ArbMode { PRIORITY = 0, ROUND_ROBIN = 1 };
// String representation of ports for logging purposes
const char* PortNames[] = { "NORTH", "SOUTH", "EAST", "WEST" };

struct packet {
    enum Type { 
        REQ_WRITE = 0, // CPU requests to write data to MEM
        REQ_READ = 1,  // CPU requests to read data from MEM
        RSP_ACK = 2,   // MEM confirms data has been written
        RSP_DATA = 3   // MEM sends requested data back to CPU
    };

    Type type;           // Current message type
    int src_id;          // Initiator ID (CPU ID)
    int dst_id;          // Destination ID (MEM ID)
    int address;         // Memory address for read/write
    int data;            // Actual data (for WRITE or RSP_DATA)
    int ttl;             // Time To Live: prevents infinite loops
    sc_time birth_time;  // Timestamp used to calculate end-to-end latency

    // Default Constructor
    packet() : type(REQ_WRITE), src_id(0), dst_id(0), address(0), data(0) {
        ttl = 10;                     
        birth_time = sc_time_stamp();
    }

    // Parameterized Constructor
    packet(Type t, int s, int d, int addr, int val) 
        : type(t), src_id(s), dst_id(d), address(addr), data(val) {
        ttl = 10;                      
        birth_time = sc_time_stamp();
    }

    // Equality operator
    bool operator==(const packet& other) const {
        return (type == other.type && src_id == other.src_id && 
                dst_id == other.dst_id && address == other.address && 
                data == other.data && ttl == other.ttl);
    }
    
    // Stream operator for easy logging
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
           << " Addr:" << p.address << " Data:" << p.data 
           << " TTL:" << p.ttl << "]";
        return os;
    }
};

struct cfg_trans {
    enum Type { SET_ROUTE = 0, ENABLE_PORT = 1, SET_Q_LEN = 2, SET_ARBITER = 3 };

    int type;   // Command type
    int target; // For SET_ROUTE: destination ID; For ENABLE_PORT: port ID
    int value;  // For SET_ROUTE: output port ID; For SET_ARBITER: 0=FixedPriority, 1=RR

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