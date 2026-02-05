# Network-on-Chip

## Introduction

NoC-SystemC is a modular, cycle-accurate simulator for a Network-on-Chip (NoC) architecture, implemented using C++ and the SystemC library.

This project simulates a packet-switched network designed to interconnect various IP blocks (CPUs and Memories) on a chip. It provides a highly configurable environment to test routing algorithms, arbitration policies, and network topologies (Mesh/Torus). The system is capable of generating detailed performance metrics, including end-to-end latency, throughput analysis, and congestion monitoring.

## Project Description

The core of the simulation is a generic, configurable Router module connected via bidirectional channels. The system scales dynamically based on a JSON configuration file, allowing the instantiation of complex topologies ranging from simple 2x2 meshes to large 6x6 Torus grids (36+ routers).

The simulation models the entire lifecycle of a transaction:

1. **Configuration**: A Configurator module parses a JSON file to set up routing tables, buffer sizes, and arbitration policies.

2. **Traffic Generation**: CPU modules generate read/write requests.

3. **Routing**: Packets traverse the network using adaptive routing logic.

4. **Execution**: Memory modules process requests and send acknowledgments back to the source.

5. **Analysis**: The system tracks Time-To-Live (TTL), latency, and packet drops for performance visualization.


## System Architecture

- **Scalability**: Supports a minimum of 8 routers, scalable to 36+ (Torus 6x6).

- **Connectivity**: defined via configuration files (JSON).

- **Flexible Topology**: Routers, CPUs, and Memories can be connected in arbitrary layouts defined by the user.


## Core Components

### 1. Shared Data Structures (```utils.h```)

#### Overview

The utils.h header serves as the protocol definition layer for the NoC simulator. It contains the data structures required for both the *Data Plane* (actual memory traffic) and the *Control Plane* (network configuration).

#### Key Data Structure

1. The ```packet``` Structure Encapsulates all information necessary for end-to-end communication between CPUs and Memories.

- **Header**: Contains ```type```, ```src_id```, and ```dst_id```. There are four types of transactions:
```C++
enum Type { 
        REQ_WRITE = 0, // CPU requests to write data to MEM
        REQ_READ = 1,  // CPU requests to read data from MEM
        RSP_ACK = 2,   // MEM confirms data has been written
        RSP_DATA = 3   // MEM sends requested data back to CPU
    };
```
- **Payload**: Carries the memory ```address``` and the actual ```data```.

- **Telemetry**:

   - **TTL (Time To Live)**: Integer decremented at each hop to prevent infinite loops.

   - **Birth Time**: sc_time object used to measure precise end-to-end latency.

2. Configuration Transaction (struct cfg_trans) Defines the structure for control signals sent via the configuration bus during the initialization phase.

Command Types:

- **SET_ROUTE**: Updates the routing table (maps Destination ID → Output Port).

- **ENABLE_PORT**: Activates or deactivates a physical port (Fault injection simulation).

- **SET_ARBITER**: Switches between Fixed Priority and Round-Robin arbitration.

- **SET_Q_LEN**: Sets the maximum depth of input FIFOs.

#### Helper Utilities

- **Port Mapping**: ```PortID``` enum maps ```{N, S, E, V}``` to ```{0, 1, 2, 3}``` for array indexing.

- **Stream Operators**: Overloaded ```operator<<``` for both structures enables human-readable logging to ```std::cout``` for debugging and trace generation.

<img width="782" height="421" alt="image" src="https://github.com/user-attachments/assets/56c5c1d0-4a9c-4295-aa63-8fb654ce53a2" />

### 2. Router (```router.h```)

The Router is a cycle-accurate SystemC module responsible for packet switching, flow control, and network management. It operates on a hop-by-hop basis, using internal routing tables and arbitration logic to forward traffic from source to destination.

#### Internal Mechanisms
- **Arbitration Engine**: Implements two policies:

   - ```PRIORITY```: Strict port ordering (N > S > E > W).

   - ```ROUND_ROBIN```: Dynamic priority rotation to prevent port starvation.

- **Adaptive Routing**: Supports multipath forwarding. If the ```routing_table``` provides multiple output vectors for a single ```dst_id```, the router performs a load-balancing check, selecting the port with the highest available FIFO capacity (```num_free()```).

- **Error Handling & Telemetry**:

   - **TTL Enforcement**: Drops packets with expired lifetimes to mitigate routing loops.

   - **Drop Counters**: Categorizes and counts failed transmissions (No Route, Disabled Port, TTL Expired) for post-simulation analysis.

#### Configuration commands
The router processes cfg_trans objects to update its behavior at runtime:

- ```SET_ROUTE```: Appends a new port to a destination's vector list.

- ```ENABLE_PORT```: Toggles physical link availability.

- ```SET_ARBITER```: Changes the arbitration logic between Priority and Round Robin.

#### Router WorkFlow Process

![Example_page-0001](https://github.com/user-attachments/assets/bc7a39b3-73c4-4a2d-b817-f8bdf8bca8cc)

### 3. CPU Master Module (```cpu_v2.h```)

#### Overview
The ```CPU_L2``` module simulates a processing unit that acts as a traffic generator for the Network-on-Chip. It operates in a blocking, synchronous mode, meaning it issues a memory request and halts execution until the corresponding response is received from the target memory.

#### Internal Architecture
1. **Task Queue** (```tasks```): A FIFO buffer storing ```CpuTask``` objects. This allows the simulation to run deterministic traffic patterns defined in the JSON configuration.

2. **Performance Counters**:

   - ```total_latency```: Accumulates the round-trip time of all completed transactions.

   - ```received_packets```: Counts successful operations to calculate the average latency at the end of the simulation.

#### Logic Flow: ```process()```

The module runs a dedicated ```SC_THREAD``` with the following cycle:

- **Fetch**: Retrieves the next instruction from the queue.

- **Delay**: Executes a ```wait()``` to model internal processing time or bus idle time.

- **Issue**: Constructs a packet (Header + Payload) and pushes it to the ```out_port```.

- **Wait** (Blocking): Calls ```in_port.read()```, suspending the thread until the NoC delivers the response.

- **Telemetry**: Upon wake-up, calculates latency (```Current Time - Packet Birth Time```) and logs the result.

### 4. Memory Module (```mem.h```)

#### Overview
The ```MEM``` module is a functional SystemC model of a target memory slave within the NoC. It processes incoming transactions (Read/Write) and issues appropriate responses, simulating local storage using an associative array.

#### Key Functional Behaviors
- **Blocking Execution**: The module utilizes an ```SC_THREAD``` that suspends on ```in_port.read()```, ensuring it only consumes simulation cycles when data is present.

- **Memory Modeling**: Internal storage is implemented via ```std::map<int, int>```, allowing for a flexible, sparse memory map where addresses are allocated dynamically upon the first write.

- **Transaction Handling**:

   - **Write**: Updates the internal map and issues an ```RSP_ACK``` to provide flow control confirmation to the initiator.

   - **Read**: Performs a lookup in the map and returns an ```RSP_DATA``` packet containing the stored value or a default zero.

- **Timing Simulation**: A fixed delay of 10ns is introduced before writing the response to the output port, modeling the physical access time of the memory hardware.

<img width="600" height="550" alt="image" src="https://github.com/user-attachments/assets/b0040342-fa85-4aee-bfe6-40f8ccddd7d9" />

---

## Development Levels

This project was designed and implemented to meet specific academic requirements, progressing through four levels of complexity (L0 - L3).

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

## Key Features

- **Adaptive Routing**: The router intelligently selects output ports based on buffer occupancy (congestion awareness).

- **Traffic & Congestion Analysis**: Generates CSV reports and Python-based visualizations (Heatmaps, Bar Charts) to analyze network hotspots.

- **Robustness**: Handles routing loops via TTL and disabled ports gracefully.

- **JSON Configuration**: Uses nlohmann/json for modern, human-readable configuration of the entire topology.

## Visual Reports

The simulation outputs a performance_report.csv which is processed to generate visual analytics:

- **Traffic Heatmap**: Visualizes load distribution across the Torus grid.

- **Latency Charts**: Shows average transaction time per CPU.

- **Drop Analysis**: Highlights packets dropped due to TTL expiration or disabled routes.


## Technical Details

### Prerequisites
* **C++ Compiler** (GCC 7+ or Clang)
* **SystemC** 2.3.x library installed on your system.
* **Python 3** (for visualization scripts) with pandas, matplotlib, and seaborn.

