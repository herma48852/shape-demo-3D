# 3D Shapes Demo

A high-performance, real-time 3D network telemetry visualization suite built around **RTI Connext DDS 7.7.0** using the Modern C++ API (`cpp2_api`). The application utilizes hardware-accelerated **OpenGL** and **GLFW3** for 3D graphics rendering, integrated with a dynamic, stateful graphical user interface powered by **Dear ImGui**.

This project provides a fully localized distributed network simulation layer allowing developers to publish and subscribe to geometric spatial telemetry topics across an automated build and code-generation pipeline.

---

## 🚀 Key Features

* **Distributed 3D Telemetry Orchestration:** Native integration with RTI Connext DDS 7.7.0 enabling seamless serialization, network publication, and distributed discovery of 3D spatial coordinate updates.
* **Automated `rtiddsgen` IDL Compilation:** Integrates an extensible CMake compilation pipeline that automatically monitors and compiles standard Interface Definition Language (`.idl`) schemas into strongly-typed modern C++ source and plugin bindings.
* **Hardware-Accelerated 3D Renderer:** Legacy OpenGL context wrapper capable of drawing wireframe and solid geometries (Cubes, Spheres, Tetrahedrons) with fully integrated alpha-fading chronological history trails.
* **Dynamic QoS Engineering:** Interactive sliders let you change Quality of Service (QoS) parameters at runtime, such as **Time-Based Filters** and **History Depth Profiles**, instantly modulating incoming middleware throughput.
* **Isolated Network Transport Layer:** Enforces explicit network isolation by disabling shared memory transports and binding the middleware exclusively to the UDPv4 Local Loopback interface (`127.0.0.1`) via specialized participant property keys.
* **Interactive Orbital Camera Matrix:** High-fidelity viewport navigation featuring left-click rotation tracking, right-click spatial panning, and scrolling zoom metrics, with automatic UI pointer focus capture overrides.

---

## 📂 Project Architecture & Component Mapping

```text
├── CMakeLists.txt              # Core build coordination & third-party dependency tracking 
├── build.sh                    # Automated shell bootstrap script for macOS environments 
├── imgui.ini                   # Serialized application layout configuration state 
└── src/                        # Main application source repository 
    ├── main.cpp                # App bootstrapping, orchestration, and structural loop execution 
    ├── thread_queue.hpp        # Lock-guarded thread-safe buffer for sample dispatch isolation 
    ├── dds/
    │   ├── dds_manager.hpp     # Abstraction header for the Connext participant & lifecycle groups 
    │   └── dds_manager.cpp     # Transport properties injection, Writer/Reader instantiations 
    ├── graphics/
    │   ├── graphics_engine.hpp # Context and state declarations for viewport and rendering 
    │   └── graphics_engine.cpp # Raw OpenGL shape drawing and cursor interaction matrices 
    └── idl/
        └── ShapeType.idl       # OMG-compliant extensible schema definitions for shape structures 
```

### 1. Middleware Layer (`src/dds/`)
* **`DdsSubsystemManager`:** Manages the lifecycle of core DDS entities including `DomainParticipant`, `Publisher`, `Subscriber`, and collections of dynamic `DataWriter` and `DataReader` endpoints.
* **`ShapeTypeListener`:** Inherits from `dds::sub::NoOpDataReaderListener<ShapeTypeExtended>`. Asynchronously handles data available signals, intercepts incoming payloads via `reader.take()` , verifies validation parameters , and pipes valid samples to the cross-thread buffer.
* **Transport Hardening:** Injecting specific properties (`dds.transport.UDPv4.builtin.parent.allow_interfaces` set to `127.0.0.1`)  ensures that data stays localized, eliminating unintended cross-network discovery interference.

### 2. Graphics Infrastructure (`src/graphics/`)
* **`GraphicsEngine`:** Bootstraps the GLFW framework, sets up an OpenGL modelview projection window matrix, and orchestrates primitive 3D object rendering.
* **Interaction Isolation:** Incorporates conditional checks (`ImGui::GetIO().WantCaptureMouse`) to ensure that workspace camera pan or rotation inputs are suppressed when interacting with the ImGui control panels.

### 3. Thread-Safe Dispatch (`src/thread_queue.hpp`)
* **`ThreadSafeSampleQueue`:** A lock-guarded double-buffer wrapper utilizing standard sequential vectors (`std::vector`). Provides non-blocking serialization decoupling by isolating middleware background transport processing threads from the primary 60Hz graphics rendering loop using transactional drainage.

---

## 🧬 Data Model Specification (`src/idl/ShapeType.idl`)

The telemetry payloads map standard RTI legacy shapes demo parameters, extended natively via `@Extensibility EXTENSIBLE_EXTENSIBILITY` rules to capture full 3D coordinates and rotational transformations:

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
1. **RTI Connext DDS 7.7.0** (Must be installed at default directory levels or mapped explicitly via environment variables).
2. **CMake v3.20+**.
3. **GLFW3 Development Libraries** (e.g., via Homebrew on macOS: `brew install glfw`).
4. **OpenGL Frame Contexts**.

### Automated Build (macOS Environments)
The repository provides an automated bootstrap script that handles SDK pathing exports, environment architectural sourcing, and compiler diagnostic bypass configurations:

```bash
# Provide executable permissions to the script
chmod +x build.sh

# Run the unified build sequence
./build.sh
```

### Manual Cross-Platform Build Sequence
To build manually on alternative operating systems, ensure your local `NDDSHOME` variable points to your RTI Connext installation directory:

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

Once successfully compiled, run the executable binary inside the build output repository:

```bash
./rti_3d_shapes [Domain_ID]
```
* *Note: Passing a numerical parameter chooses the target DDS domain. If no arguments are supplied, the system defaults to Domain `0`.*

### Spatial Navigation Control Matrix
When interacting inside the hardware-accelerated 3D viewport, use the following control mechanics:
* **Left-Click + Mouse Drag:** Orbitally rotates the camera perspective view around the central 3D space.
* **Right-Click + Mouse Drag:** Transports and pans the camera view along the orthogonal spatial grid.
* **Mouse Scroll Wheel:** Adjusts the dynamic focal distance (Zoom Scale) targeting localized structural views.

### Dynamic Control UI Interface Overview
The graphical window divided within the visualizer provides real-time access to the underlying DDS bus configuration:
1. **DataWriters Control Panel:** Select a target shape topic (`Square`, `Circle`, `Triangle`), choose an identifying color signature, specify scale sizes, configure kinematic trajectory tracking patterns (`PLANE_Z125`, `BOUNCE_3D`, `ORBIT`), select the publish frequency (Hz), and press initialize to bind active publishing entities onto the local loopback bus.
2. **DataReaders Filter Configuration:** Set global Quality of Service variables on-the-fly, including **Time Filters** to throttle minimum structural update boundaries or **History Depth** constraints to manage chronologically cached data samples. Custom filters can be instantiated targeting unique combinations of shape topics and color streams.

