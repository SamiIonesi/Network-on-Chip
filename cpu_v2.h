#ifndef CPU_L2_H
#define CPU_L2_H

#include <systemc.h>
#include <queue>
#include "utils.h"

// Structure for a CPU instruction
struct CpuTask {
    int type;      // REQ_WRITE / REQ_READ
    int target_id; // Destination (MEM ID)
    int addr;      // Memory address
    int data;      // Data (only for Write)
    int delay;     // Delay before execution (ns)
};

SC_MODULE(CPU_L2) {
    sc_fifo_out<packet> out_port; // Sends requests
    sc_fifo_in<packet>  in_port;  // Receives responses

    int my_id;
    std::queue<CpuTask> tasks; // Instruction queue (CPU "Program")

    // L3 Performance Statistics
    sc_time total_latency = SC_ZERO_TIME; // Cumulative round-trip time
    int received_packets = 0;             // Total responses received
    int timeouts = 0;                     // Total requests that timed out

    // Timeout Configuration
    const int TIMEOUT_LIMIT = 600; // Nanoseconds to wait before giving up
    const int CHECK_INTERVAL = 10; // How often to check the port (ns)

    void process() {
        wait(10, SC_NS); // Startup delay

        while (!tasks.empty()) {
            // 1. Get the next instruction
            CpuTask t = tasks.front();
            tasks.pop();

            // 2. Wait if necessary (Configurable delay)
            if (t.delay > 0) wait(t.delay, SC_NS);

            // 3. Execute WRITE or READ
            packet req(static_cast<packet::Type>(t.type), my_id, t.target_id, t.addr, t.data);

            if (t.type == packet::REQ_WRITE) {
                cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] WRITE -> MEM " << t.target_id 
                     << " [Addr:" << t.addr << " Val:" << t.data << "]" << endl;
            } 
            else {
                cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] READ  -> MEM " << t.target_id 
                     << " [Addr:" << t.addr << "]" << endl;
            }

            // Send the packet
            out_port.write(req);

            // 4. Wait for Confirmation with TIMEOUT mechanism
            bool success = false;
            int time_waited = 0;

            while (time_waited < TIMEOUT_LIMIT) {
                // Check if data is available in the FIFO
                if (in_port.num_available() > 0) {
                    packet rsp;
                    in_port.read(rsp); // Non-blocking read (we know data is there)

                    // L3 Latency Calculation
                    sc_time latency = sc_time_stamp() - rsp.birth_time;
                    total_latency += latency;
                    received_packets++;

                    if (rsp.type == packet::RSP_DATA) {
                        cout << "      [CPU " << my_id << "] RECV DATA: " << rsp.data 
                             << " (Latency: " << latency << ")" << endl;
                    } else if (rsp.type == packet::RSP_ACK) {
                        cout << "      [CPU " << my_id << "] RECV ACK." 
                             << " (Latency: " << latency << ")" << endl;
                    }
                    success = true;
                    break; // Exit the wait loop
                }

                // Wait a bit before checking again
                wait(CHECK_INTERVAL, SC_NS);
                time_waited += CHECK_INTERVAL;
            }

            // 5. Handle Timeout
            if (!success) {
                cout << "@" << sc_time_stamp() << " [CPU " << my_id 
                     << "] TIMEOUT! No response from MEM " << t.target_id 
                     << " (Waited " << TIMEOUT_LIMIT << "ns)" << endl;
                timeouts++;
            }
        }
        cout << "@" << sc_time_stamp() << " [CPU " << my_id << "] Finished all tasks." << endl;
    }

    // Displays final performance metrics for this CPU
    void print_cpu_stats() {
        cout << "-------------------------------------------" << endl;
        cout << " Statistics for CPU: " << my_id << endl;
        if (received_packets > 0) {
            cout << "  - Total Responses: " << received_packets << endl;
            cout << "  - Average Latency: " << total_latency / received_packets << endl;
        } else {
            cout << "  - No packets received." << endl;
        }
        // New Metric
        if (timeouts > 0) {
            cout << "  - TIMEOUTS (Lost Packets): " << timeouts << endl;
        }
        cout << "-------------------------------------------" << endl;
    }

    SC_HAS_PROCESS(CPU_L2);
    CPU_L2(sc_module_name name, int id) : sc_module(name), my_id(id) {
        SC_THREAD(process);
    }
    
    // Function used by Configurator to add instructions
    void add_task(int type, int target, int addr, int data, int delay) {
        tasks.push({type, target, addr, data, delay});
    }
};
#endif