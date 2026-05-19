# RTI Connext DDS 3D Shapes Workspace & Visualizer

[cite_start]A high-performance, real-time 3D network telemetry workspace and visualization suite built around **RTI Connext DDS 7.7.0** using the Modern C++ API (`cpp2_api`)[cite: 42, 54]. [cite_start]The application utilizes hardware-accelerated **OpenGL** and **GLFW3** for 3D graphics rendering [cite: 44][cite_start], integrated with a dynamic, stateful graphical user interface powered by **Dear ImGui**[cite: 52].

This project provides a fully localized distributed network simulation layer allowing developers to publish and subscribe to geometric spatial telemetry topics across an automated build and code-generation pipeline.

---

## 🚀 Key Features

* [cite_start]**Distributed 3D Telemetry Orchestration:** Native integration with RTI Connext DDS 7.7.0 enabling seamless serialization, network publication, and distributed discovery of 3D spatial coordinate updates[cite: 42, 734].
* [cite_start]**Automated `rtiddsgen` IDL Compilation:** Integrates an extensible CMake compilation pipeline that automatically monitors and compiles standard Interface Definition Language (`.idl`) schemas into strongly-typed modern C++ source and plugin bindings[cite: 50, 51].
* [cite_start]**Hardware-Accelerated 3D Renderer:** Legacy OpenGL context wrapper capable of drawing wireframe and solid geometries (Cubes, Spheres, Tetrahedrons) with fully integrated alpha-fading chronological history trails[cite: 507, 508, 509, 514, 515, 516, 520].
* [cite_start]**Dynamic QoS Engineering:** Interactive sliders let you change Quality of Service (QoS) parameters at runtime, such as **Time-Based Filters** and **History Depth Profiles**, instantly modulating incoming middleware throughput[cite: 498, 503, 543].
* [cite_start]**Isolated Network Transport Layer:** Enforces explicit network isolation by disabling shared memory transports and binding the middleware exclusively to the UDPv4 Local Loopback interface (`127.0.0.1`) via specialized participant property keys[cite: 731, 732, 733].
* [cite_start]**Interactive Orbital Camera Matrix:** High-fidelity viewport navigation featuring left-click rotation tracking, right-click spatial panning, and scrolling zoom metrics, with automatic UI pointer focus capture overrides[cite: 595, 650, 652, 655, 659].

---

## 📂 Project Architecture & Component Mapping

```text
├── CMakeLists.txt              # Core build coordination & third-party dependency tracking [cite: 38]
├── build.sh                    # Automated shell bootstrap script for macOS environments [cite: 165]
├── imgui.ini                   # Serialized application layout configuration state [cite: 226]
└── src/                        # Main application source repository [cite: 293]
    ├── main.cpp                # App bootstrapping, orchestration, and structural loop execution [cite: 424]
    ├── thread_queue.hpp        # Lock-guarded thread-safe buffer for sample dispatch isolation [cite: 356]
    ├── dds/
    │   ├── dds_manager.hpp     # Abstraction header for the Connext participant & lifecycle groups [cite: 776]
    │   └── dds_manager.cpp     # Transport properties injection, Writer/Reader instantiations [cite: 716]
    ├── graphics/
    │   ├── graphics_engine.hpp # Context and state declarations for viewport and rendering [cite: 581]
    │   └── graphics_engine.cpp # Raw OpenGL shape drawing and cursor interaction matrices [cite: 618]
    └── idl/
        └── ShapeType.idl       # OMG-compliant extensible schema definitions for shape structures [cite: 821]
```

### 1. Middleware Layer (`src/dds/`)
* [cite_start]**`DdsSubsystemManager`:** Manages the lifecycle of core DDS entities including `DomainParticipant`, `Publisher`, `Subscriber`, and collections of dynamic `DataWriter` and `DataReader` endpoints[cite: 734, 787, 790].
* [cite_start]**`ShapeTypeListener`:** Inherits from `dds::sub::NoOpDataReaderListener<ShapeTypeExtended>`[cite: 720, 784]. [cite_start]Asynchronously handles data available signals, intercepts incoming payloads via `reader.take()` [cite: 720, 721][cite_start], verifies validation parameters [cite: 721][cite_start], and pipes valid samples to the cross-thread buffer[cite: 723].
* [cite_start]**Transport Hardening:** Injecting specific properties (`dds.transport.UDPv4.builtin.parent.allow_interfaces` set to `127.0.0.1`) [cite: 732] [cite_start]ensures that data stays localized, eliminating unintended cross-network discovery interference[cite: 731, 733].

### 2. Graphics Infrastructure (`src/graphics/`)
* [cite_start]**`GraphicsEngine`:** Bootstraps the GLFW framework, sets up an OpenGL modelview projection window matrix, and orchestrates primitive 3D object rendering[cite: 589, 591, 592, 593, 594, 631, 636].
* [cite_start]**Interaction Isolation:** Incorporates conditional checks (`ImGui::GetIO().WantCaptureMouse`) to ensure that workspace camera pan or rotation inputs are suppressed when interacting with the ImGui control panels[cite: 649, 656, 658].

### 3. Thread-Safe Dispatch (`src/thread_queue.hpp`)
* [cite_start]**`ThreadSafeSampleQueue`:** A lock-guarded double-buffer wrapper utilizing standard sequential vectors (`std::vector`)[cite: 362]. [cite_start]Provides non-blocking serialization decoupling by isolating middleware background transport processing threads from the primary 60Hz graphics rendering loop using transactional drainage[cite: 364, 365, 487].

---

## 🧬 Data Model Specification (`src/idl/ShapeType.idl`)

[cite_start]The telemetry payloads map standard RTI legacy shapes demo parameters, extended natively via `@Extensibility EXTENSIBLE_EXTENSIBILITY` rules to capture full 3D coordinates and rotational transformations[cite: 821, 830]:

```idl
enum ShapeFillKind {
    SOLID_FILL,
    TRANSPARENT_FILL,
    HORIZONTAL_HATCH_FILL,
    VERTICAL_HATCH_FILL
};

struct ShapeType {
    string<128> color; // @key Identifier attribute for instance tracking
    long x;
    long y;
    long shapesize;
};

struct ShapeTypeExtended : ShapeType {
    ShapeFillKind fillKind;
    float angle;       // Kinematic rotational angle around arbitrary axial paths
    long z;           // Native 3D height coordination factor
};
```

---

## 🛠️ Compilation & Local Building Instructions

### Prerequisites
Ensure your host machine has the following packages installed:
1. [cite_start]**RTI Connext DDS 7.7.0** (Must be installed at default directory levels or mapped explicitly via environment variables)[cite: 42, 166].
2. [cite_start]**CMake v3.20+**[cite: 42].
3. [cite_start]**GLFW3 Development Libraries** (e.g., via Homebrew on macOS: `brew install glfw`)[cite: 48, 165].
4. [cite_start]**OpenGL Frame Contexts**[cite: 44].

### Automated Build (macOS Environments)
[cite_start]The repository provides an automated bootstrap script that handles SDK pathing exports, environment architectural sourcing, and compiler diagnostic bypass configurations[cite: 165]:

```bash
# Provide executable permissions to the script
chmod +x build.sh

# Run the unified build sequence
./build.sh
```

### Manual Cross-Platform Build Sequence
[cite_start]To build manually on alternative operating systems, ensure your local `NDDSHOME` variable points to your RTI Connext installation directory[cite: 42, 166]:

```bash
# 1. Instantiate environmental path mappings
export NDDSHOME="/opt/rti_connext_dds-7.7.0"  # Adapt to match your target file system path

# 2. Allocate and enter transient build directory
mkdir build && cd build

# 3. Configure the platform-specific build engine using CMake
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCONNEXTDDS_DIR="$NDDSHOME" \
      -DCONNEXTDDS_ALLOW_UNSUPPORTED_COMPILER=ON \
      ..

# 4. Invoke the compilation engine
cmake --build . --config Release
```

---

## 🎮 Execution & Runtime Operation

[cite_start]Once successfully compiled, run the executable binary inside the build output repository[cite: 182]:

```bash
./rti_3d_shapes [Domain_ID]
```
* [cite_start]*Note: Passing a numerical parameter chooses the target DDS domain[cite: 449]. [cite_start]If no arguments are supplied, the system defaults to Domain `0`[cite: 449].*

### Spatial Navigation Control Matrix
When interacting inside the hardware-accelerated 3D viewport, use the following control mechanics:
* [cite_start]**Left-Click + Mouse Drag:** Orbitally rotates the camera perspective view around the central 3D space[cite: 650, 654].
* [cite_start]**Right-Click + Mouse Drag:** Transports and pans the camera view along the orthogonal spatial grid[cite: 652, 655].
* [cite_start]**Mouse Scroll Wheel:** Adjusts the dynamic focal distance (Zoom Scale) targeting localized structural views[cite: 659].

### Dynamic Control UI Interface Overview
[cite_start]The graphical window divided within the visualizer provides real-time access to the underlying DDS bus configuration[cite: 527]:
1. [cite_start]**DataWriters Control Panel:** Select a target shape topic (`Square`, `Circle`, `Triangle`), choose an identifying color signature, specify scale sizes, configure kinematic trajectory tracking patterns (`PLANE_Z125`, `BOUNCE_3D`, `ORBIT`), select the publish frequency (Hz), and press initialize to bind active publishing entities onto the local loopback bus[cite: 528, 529, 531].
2. [cite_start]**DataReaders Filter Configuration:** Set global Quality of Service variables on-the-fly, including **Time Filters** to throttle minimum structural update boundaries or **History Depth** constraints to manage chronologically cached data samples[cite: 543]. [cite_start]Custom filters can be instantiated targeting unique combinations of shape topics and color streams[cite: 545, 547].

