# Network-on-Chip (NoC) Simulation Environment

A high-level cycle-accurate simulation of a Network-on-Chip (NoC) architecture implemented in **SystemC**. This project models a packet-switched communication network between processing units (CPUs) and memory controllers.

## 🏗️ System Architecture

The environment follows a modular hardware-centric design, utilizing SystemC's `SC_MODULE` for hardware components and `sc_fifo` for communication channels.

### 1. Core Components
* **CPU (Traffic Generator):** Acts as a Bus Master. It initiates memory transactions (Read/Write) and implements a blocking handshake mechanism. It performs self-checking by verifying if data read matches the data previously written.
* **Router (Switch):** The heart of the NoC. It features a 4-port (North, South, East, West) architecture with an internal arbiter. It uses a look-up table (LUT) to decide the output port based on the `dst_id` of the incoming packet.
* **MEM (Memory Slave):** Simulates a memory controller using a `std::map<int, int>`. This allows for sparse memory allocation (modeling a large address space without high RAM usage on the host machine).
* **Packet (Transaction Layer):** A custom data structure containing:
    * `type`: REQ_READ, REQ_WRITE, RSP_DATA, RSP_ACK.
    * `src_id` / `dst_id`: Routing metadata.
    * `address` / `data`: Payload information.

### 2. Communication Protocol
The system implements a **Request-Response Handshake**:
1.  **Request Phase:** CPU sends a `REQ` packet through its `out_port`.
2.  **Routing Phase:** Routers forward the packet hop-by-hop based on static routing tables.
3.  **Processing Phase:** MEM receives the request, performs the operation, and generates a `RSP` packet.
4.  **Response Phase:** The response packet travels back to the specific CPU that initiated the request.

---

## 📈 Development Levels

### Level 0: Unit Testing
- Validation of a single Router instance.
- Verified arbitration logic and port-to-port packet forwarding.

### Level 1: Static Topology (Current)
- **Scale:** 8-Router linear backbone.
- **Complexity:** Complex topology with multiple peripheral devices (CPUs/MEMs).
- **Routing:** Manual configuration of routing tables via a dedicated `cfg_port`.
- **Validation:** Successful end-to-end communication from Router 0 (West) to Router 7 (East) with a 10ns simulated memory access latency.

### Level 2: Dynamic System (Planned)
- **Configuration:** Introduction of a **System Configurator** module.
- **Parsing:** Automatic network assembly by reading an external configuration file (Topology & Traffic).
- **Scalability:** Support for 16+ routers and configurable FIFO queue depths.

---

## 🛠️ Technical Details

### Prerequisites
* C++ Compiler (GCC 7+ or Clang)
* SystemC 2.3.x library installed on your system.

### Build and Run
Set your `SYSTEMC_HOME` environment variable, then compile:

```bash
g++ -I$SYSTEMC_HOME/include -L$SYSTEMC_HOME/lib-linux64 \
    -o noc_sim L1_network.cpp -lsystemc -lm