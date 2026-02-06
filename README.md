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

<img width="594" height="679" alt="image" src="https://github.com/user-attachments/assets/04efc424-fca4-4896-8e5c-a987b953cb32" />

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

## 5. System Configurator (`Configurator.h`)

### Overview
The `Configurator` class serves as the initialization engine for the NoC Simulator. It parses the system architecture defined in a JSON file and populates internal data structures used by the main simulation loop to instantiate and connect modules.

### Key Functionalities
* **JSON Parsing:** Utilizes the `nlohmann::json` library to deserialize complex topology definitions.
* **Topology Abstraction:** Converts high-level JSON descriptions (e.g., "North", "CPU") into low-level SystemC parameters (Port ID `0`, Module Pointers).
* **L3 Multipath Support:**
    * Detects whether a routing entry specifies a single path or multiple paths.
    * Populates the `RouteDef` structure with a vector of viable output ports, enabling **Adaptive Routing** in the routers.
* **Error Handling:** Wraps parsing logic in `try-catch` blocks to gracefully report malformed JSON files without crashing the simulation.

### Data Structures
* **`LinkDef`**: Defines physical connections between routers (`src_r` ↔ `dst_r`).
* **`DeviceDef`**: Specifies peripheral attachment points and CPU instruction sets.
* **`RouteDef`**: Maps destination IDs to one or more output ports for a specific router.

<img width="666" height="370" alt="image" src="https://github.com/user-attachments/assets/e2a1fda7-cc17-4492-b0f2-879a0f25acb8" />

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

### Level 2: Dynamic System
- **Configuration:** Introduction of a **System Configurator** module (`Configurator.h`).
- **Parsing:** Automatic network assembly by reading an external JSON configuration file (`config_L2.json`).
- **Scalability:** Validated support for 16+ routers (4x4 Mesh) with variable FIFO queue depths and arbitration policies.
- **Flexibility:** Topology changes (e.g., modifying links or device placement) require only a text file update, not a code recompilation.

#### Visual Representation
The system now parses a JSON structure to instantiate the hardware. The diagram below illustrates a 4x4 Mesh topology generated dynamically.

#### Simulation Output Log Example
The log demonstrates the Configurator parsing the JSON and initializing a 16-router mesh.

```bash
--- START L2 SIMULATION ---
[SYS] Loading configuration from config_L2.json...
[SYS] Parsed 16 routers.
[SYS] Parsed 24 links.
[SYS] Parsed 4 devices.
[SYS] Configuring Routing Tables...
@10 ns [CFG Router_0] Added Route: Dst 200 -> Port EAST
@10 ns [CFG Router_1] Added Route: Dst 200 -> Port SOUTH
...
@10 ns [CPU 50] WRITE -> MEM 200 [Addr:20 Val:999]
@20 ns [ROUTER Router_0] Pkt in port NORTH: [WRITE Src:50 -> Dst:200 ...] -> Fwd to Port EAST
@30 ns [ROUTER Router_1] Pkt in port WEST:  [WRITE Src:50 -> Dst:200 ...] -> Fwd to Port SOUTH
@40 ns [ROUTER Router_5] Pkt in port NORTH: [WRITE Src:50 -> Dst:200 ...] -> Fwd to Port EAST
@50 ns [MEM 200] RECV: [WRITE Src:50 -> Dst:200 Addr:20 Data:999]
    ---> [WRITE OP] Written value 999 at address 20
    ---> [REPLY] Sending response to CPU 50
@70 ns [CPU 50] RECV ACK. (Latency: 60 ns)
--- END L2 SIMULATION ---
```

### Level 3: Advanced Network-on-Chip Features

#### Overview

This level implements advanced Network-on-Chip (NoC) features using a **6x4 Torus topology** with horizontal wraparound links.

The design focuses on adaptive routing, fault handling, and detailed performance monitoring.

---

#### Network Topology

- **Topology:** 6x4 Torus (24 Routers)
- **Wraparound Links:** Horizontal (Left ↔ Right columns)
- **Total Routers:** 24

This configuration enables direct edge-to-edge communication, reducing hop count and latency.

---

#### L3 Features

##### Adaptive Routing (Multipath)

Routers utilize a `vector<int>` to store available output ports.

- If a port becomes congested or disabled
- The arbiter dynamically selects an alternative path
- Improves reliability and load balancing

##### TTL (Time-To-Live)

Each packet includes a TTL counter.

- Decrements at every hop
- Packets with `TTL = 0` are dropped
- Prevents infinite routing loops in the Torus network

##### Advanced Statistics

The system provides cycle-accurate reporting of:

- Routed packets
- Reroutes
- TTL drops

This allows precise performance analysis and debugging.

#### Routing Scenario

##### Scenario Description

A complex routing scenario involving:

- Horizontal wraparound traversal
- Edge-to-edge communication
- Disabled port handling

This scenario validates routing robustness under constrained conditions.

#### Visual Representation

The diagram below illustrates the 6x4 Torus architecture, including CPU/MEM placement and wraparound links.

<img width="1141" height="692" alt="image" src="https://github.com/user-attachments/assets/655b1804-3b7b-4f4a-ba03-bb6f39be00fd" />


#### Traffic Scenario: "Long Jump"

##### Goal

Transmit a packet from:

- **CPU 101** → Router 0 (North Port)
- **MEM 200** → Router 23 (South Port)

##### Path Description

Instead of traversing the internal mesh, the packet:

1. Travels South to Router 18
2. Uses the wraparound link (West)
3. Jumps directly to Router 23

This bypasses internal routing and minimizes latency.

```bash
@10 ns [CFG Router_1] Max Queue Length: 16
@10 ns [CFG Router_1] Arbiter: Round-Robin
@30 ns [CPU 101] WRITE -> MEM 200 [Addr:50 Val:999]
@40 ns [ROUTER Router_0] Pkt in port NORTH: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:10]
    -> Fwd to Port SOUTH
@50 ns [ROUTER Router_6] Pkt in port NORTH: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:9]
    -> Fwd to Port SOUTH
@60 ns [ROUTER Router_12] Pkt in port NORTH: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:8]
    -> Fwd to Port SOUTH
@70 ns [ROUTER Router_18] Pkt in port NORTH: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:7]
    -> Fwd to Port WEST
@80 ns [ROUTER Router_23] Pkt in port EAST: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:6]
    -> Fwd to Port SOUTH
@80 ns [MEM 200] RECV: [WRITE Src:101 -> Dst:200 Addr:50 Data:999 TTL:5]
      ---> [WRITE OP] Written value 999 at address 50
      ---> [REPLY] Sending response to CPU 101
@100 ns [ROUTER Router_23] Pkt in port SOUTH: [ACK   Src:200 -> Dst:101 Addr:50 Data:0 TTL:10]
    -> Fwd to Port NORTH
@110 ns [ROUTER Router_17] Pkt in port SOUTH: [ACK   Src:200 -> Dst:101 Addr:50 Data:0 TTL:9]
    -> Fwd to Port NORTH
@120 ns [ROUTER Router_11] Pkt in port SOUTH: [ACK   Src:200 -> Dst:101 Addr:50 Data:0 TTL:8]
    -> Fwd to Port NORTH
@130 ns [ROUTER Router_5] Pkt in port SOUTH: [ACK   Src:200 -> Dst:101 Addr:50 Data:0 TTL:7]
    -> Fwd to Port EAST
@140 ns [ROUTER Router_0] Pkt in port WEST: [ACK   Src:200 -> Dst:101 Addr:50 Data:0 TTL:6]
    -> Fwd to Port NORTH
      [CPU 101] RECV ACK. (Latency: 60 ns)
@140 ns [CPU 101] Finished all tasks.
--- SIMULATION FINISHED ---
```

---

## Key Features

- **Adaptive Routing**: The router intelligently selects output ports based on buffer occupancy (congestion awareness).

- **Traffic & Congestion Analysis**: Generates CSV reports and Python-based visualizations (Heatmaps, Bar Charts) to analyze network hotspots.

- **Robustness**: Handles routing loops via TTL and disabled ports gracefully.

- **JSON Configuration**: Uses nlohmann/json for modern, human-readable configuration of the entire topology.

## 6. Visual Analytics & Performance Reports

The simulation generates a raw data file (`performance_report.csv`)
containing cycle-accurate statistics for every Router and CPU. A
dedicated Python script processes this data to generate
professional-grade visualization and a textual summary.

### How to Generate Reports

Ensure you have `pandas`, `matplotlib`, and `seaborn` installed, then
run:

``` bash
python3 analyze_noc.py
```

### A. Traffic Heatmap (`report_heatmap.png`)

This 6x4 grid visualization maps the traffic intensity across the entire
Torus topology.

-   **Purpose:** Identifies network congestion and "hotspots."
-   **Observation:** In our stress test, Router 0 appears as a hotspot
    (Dark Red) due to heavy injection from CPU 101.
-   **Torus Validation:** Activity on the edge routers (Col 0 and Col 5)
    confirms that Wraparound Links are actively being used for routing.

<img width="1036" height="607" alt="image" src="https://github.com/user-attachments/assets/accd1ef3-dab7-496d-a5d8-3c5e69f555b2" />

### B. End-to-End Latency (`report_latency.png`)

Displays the average round-trip time (Request + Response) for each
active CPU.

-   **Metric:** Time elapsed from packet creation (`birth_time`) to ACK
    receipt.

**Interpretation:**

-   **CPU 101 & 102:** Show valid latency (\~50ns), confirming
    successful transactions.
-   **CPU 10 & 63:** May show no data or high timeout rates. This is
    expected behavior in our stress test, as they were assigned invalid
    routes to test the network's error handling capabilities.

<img width="1227" height="608" alt="image" src="https://github.com/user-attachments/assets/1bb0fe77-f5af-4674-acdb-a0fe4fb8d03f" />

### C. Drop Analysis & Fault Tolerance (`report_traffic_drops.png`)

A stacked bar chart that visualizes the robustness of the Layer 3
protocol. It categorizes packets into:

-   **Routed (Success):** Packets successfully forwarded.
-   **TTL Expired:** Packets dropped to prevent infinite loops (e.g.,
    the ping-pong scenario between R0 and R1).
-   **No Route:** Packets dropped because the destination address did
    not exist in the routing table (e.g., Target 999).

<img width="1488" height="735" alt="image" src="https://github.com/user-attachments/assets/b4325754-8d94-426d-bbb4-1de51d84b2bd" />

### D. Console Summary Output

The script also provides a high-level summary of the system's health.

```bash
============================================================
 📊  FULL NoC PERFORMANCE REPORT (6x4 TORUS)
============================================================

[1] GLOBAL TRAFFIC SUMMARY
  - 📦 Total Packets Sent (Est.): 58
  - ✅ Successful Deliveries:     33
  - ❌ LOST PACKETS (Total Drop): 25
      ├─ ⏳ TTL Expired:           8
      ├─ 🚫 No Destination (Route):17
      └─ 🔒 Port Disabled:         0

[2] NETWORK ACTIVITY (ROUTERS)
  - Total Hops (Switching):        284
  - Adaptive Reroutes:             24
  - Hotspot Node:                  Router_0 (33 hops)

[3] LATENCY PERFORMANCE (CPU)
  - Average Global Latency:        53933.32 ns
  - Slowest Path:                  CPU_102 (60000.00 ns)

============================================================
```

## Technical Details

### Prerequisites
* **C++ Compiler** (GCC 7+ or Clang)
* **SystemC** 2.3.x library installed on your system.
* **Python 3** (for visualization scripts) with pandas, matplotlib, and seaborn.

