![WhatsApp Image 2026-01-15 at 19 53 54](https://github.com/user-attachments/assets/8811fffa-4998-4292-a8b4-761a64a87d8e)# Network-on-Chip (NoC) Simulation Environment

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

Here is an example of how a router look's like: 

<img width="687" height="614" alt="image" src="https://github.com/user-attachments/assets/da3abb51-1d9a-4781-acf4-2791acf863d6" />

### Visual Representation
The diagram below illustrates a specific test case executed during simulation. It depicts a "stress scenario" where multiple ports inject packets simultaneously, triggering the arbiter's conflict resolution logic.

<img width="745" height="644" alt="image" src="https://github.com/user-attachments/assets/2631123e-25ab-4a00-a25f-d7e1f39a45f2" />

#### Explanation of the Diagram Events:
* **Active Routing (North):** A packet with `Dst: 20` has successfully won arbitration and is being forwarded to Port 0 (North).
* **Packet Drop (East):** A packet with `Dst: 34` arrives on Port 2 (East). Since `34` is not defined in the Routing Table (CFG), the router performs a **DROP** operation to prevent deadlock.
* **Arbitration Conflict (Center):** A packet with `Dst: 10` (from West) and `Dst: 35` (from South) both request the same resource or encounter a busy port. The Arbiter grants access to one, while the other enters a **BLOCKED** state.
* **Head-of-Line (HoL) Blocking (South):** Notice the packet `Dst: 120` on Port 1 (South). It is stuck in the FIFO queue because the packet in front of it (`Dst: 35`) is currently blocked. This validates the FIFO behavior of the input buffers.

#### Simulation Output Log
The console output confirms the cycle-accurate behavior of these events.

```bash
--- START L0  ---
@10 ns [CFG] Route: Dst 10->Port NORD
@10 ns [CFG] Route: Dst 20->Port EST
@10 ns [CFG] Route: Dst 35->Port NORD
@10 ns [CFG] Route: Dst 120->Port VEST
@10 ns [CFG] Port 3 OFF
Injecting packet 1 (on Port 3)...
Injecting packet 2 (on Port 2)...
Injecting packet 3 (on Port 0 - HIGH PRIORITY)...
Injecting packet 4 (on Port 1)...
Injecting packet 5 (on Port 1)...
@30 ns [ROUTER] Pkt in port NORD: [WRITE Src:88 -> Dst:20 Addr:5 Data:500] -> Fwd to Port EST
@40 ns [ROUTER] Pkt in port SUD: [WRITE Src:120 -> Dst:35 Addr:8 Data:40] -> Fwd to Port NORD
@50 ns [ROUTER] Pkt in port SUD: [WRITE Src:1 -> Dst:120 Addr:12 Data:43] -> DROP: Port VEST disabled
@60 ns [ROUTER] Pkt in port EST: [WRITE Src:20 -> Dst:34 Addr:0 Data:100] -> DROP: No route for Destination 34
Received on NORTH (Port 0): [WRITE Src:120 -> Dst:35 Addr:8 Data:40]
SOUTH (Port 1) is empty.
Received on EAST (Port 2): [WRITE Src:88 -> Dst:20 Addr:5 Data:500]
WEST (Port 3) is empty.
--- END ---
```

### Level 1: Static Topology
- **Scale:** 8-Router linear backbone.
- **Complexity:** Complex topology with multiple peripheral devices (CPUs/MEMs).
- **Routing:** Manual configuration of routing tables via a dedicated `cfg_port`.
- **Validation:** Successful end-to-end communication from Router 0 (West) to Router 7 (East) with a 10ns simulated memory access latency.

#### Visual Representation
The diagram below shows the end-to-end path of a packet traveling from the first router to the last router in the chain.

<img width="620" height="162" alt="image" src="https://github.com/user-attachments/assets/5aef6e19-b382-486a-b0a8-29c5c4849caf" />

#### Simulation Output Log Example
```bash
--- START L1 SIMULATION ---
@10 ns [CFG] Route: Dst 200->Port EST
@10 ns [CFG] Route: Dst 20->Port VEST
@10 ns [CFG] Route: Dst 83->Port SUD
...
@10 ns [CFG] Route: Dst 8->Port EST
@10 ns [CFG] Route: Dst 100->Port EST

@20 ns [CPU 20] INIT WRITE -> MEM 200 | Adr:10 Val:83
@30 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@40 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@50 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@60 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@70 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@80 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@90 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@100 ns [ROUTER] Pkt in port VEST: [WRITE Src:20 -> Dst:200 Addr:10 Data:83] -> Fwd to Port EST
@100 ns [MEM 200] RECV: [WRITE Src:20 -> Dst:200 Addr:10 Data:83]
      ---> [WRITE OP] Written value 83 at address 10
      ---> [REPLY] Sending response to CPU 20
@120 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@130 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@140 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@150 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@160 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@170 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@180 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@190 ns [ROUTER] Pkt in port EST: [ACK   Src:200 -> Dst:20 Addr:10 Data:0] -> Fwd to Port VEST
@190 ns [CPU 20] DONE WRITE (ACK Received)
@240 ns [CPU 20] INIT READ  -> MEM 200 | Adr:10
@250 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@260 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@270 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@280 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@290 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@300 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@310 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@320 ns [ROUTER] Pkt in port VEST: [READ  Src:20 -> Dst:200 Addr:10 Data:0] -> Fwd to Port EST
@320 ns [MEM 200] RECV: [READ  Src:20 -> Dst:200 Addr:10 Data:0]
      ---> [READ OP] Read value 83 from address 10
      ---> [REPLY] Sending response to CPU 20
@340 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@350 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@360 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@370 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@380 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@390 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@400 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@410 ns [ROUTER] Pkt in port EST: [DATA  Src:200 -> Dst:20 Addr:10 Data:83] -> Fwd to Port VEST
@410 ns [CPU 20] DONE READ (Data Received): 83
      ---> SUCCESS: Read value matches written value!
--- END L1 SIMULATION ---
```

### Level 2: Dynamic System (Planned)
- **Configuration:** Introduction of a **System Configurator** module.
- **Parsing:** Automatic network assembly by reading an external configuration file (Topology & Traffic).
- **Scalability:** Support for 16+ routers and configurable FIFO queue depths.

### Level 3: TO DO: a new future

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
