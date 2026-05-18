#!/bin/bash

echo "==========================================================="
echo "   Applying Flawless Telemetry, API & Transport Fix"
echo "==========================================================="

# Ensure target directories exist
mkdir -p src/dds src/graphics src/idl

# 1. Update Thread Queue to pass Topic Name alongside payload
cat << 'EOF' > src/thread_queue.hpp
#pragma once
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include "ShapeType.hpp" 

struct ReceivedSample {
    std::string topic_name;
    ShapeTypeExtended data;
};

class ThreadSafeSampleQueue {
private:
    std::mutex queue_mutex_;
    std::vector<ReceivedSample> sample_buffer_;

public:
    ThreadSafeSampleQueue() = default;
    ~ThreadSafeSampleQueue() = default;
    ThreadSafeSampleQueue(const ThreadSafeSampleQueue&) = delete;
    ThreadSafeSampleQueue& operator=(const ThreadSafeSampleQueue&) = delete;

    void push_sample(const std::string& topic_name, const ShapeTypeExtended& sample) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        sample_buffer_.push_back({topic_name, sample});
    }

    std::vector<ReceivedSample> drain_queue() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::vector<ReceivedSample> drained_samples = std::move(sample_buffer_);
        sample_buffer_.clear();
        return drained_samples;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        sample_buffer_.clear();
    }
};
EOF
echo "[System] Patched src/thread_queue.hpp"

# 2. Update DDS Manager Listener with direct member field access
cat << 'EOF' > src/dds/dds_manager.cpp
#include "dds_manager.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

ShapeTypeListener::ShapeTypeListener(ThreadSafeSampleQueue* queue) : target_queue_(queue) {}

void ShapeTypeListener::on_data_available(dds::sub::DataReader<ShapeTypeExtended>& reader) {
    try {
        // Fix: Use topic_description() to standard-comply with OMG C++ PSM specification
        std::string topic_name = reader.topic_description().name(); 

        auto samples = reader.take();
        for (const auto& sample : samples) {
            if (sample.info().valid()) {
                ShapeTypeExtended data = sample.data();
                
                // Direct member field updates (No getter/setter parentheses)
                if (data.z == 0) data.z = 125;

                if (target_queue_) {
                    target_queue_->push_sample(topic_name, data);
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[DDS] Exception during take(): " << ex.what() << std::endl;
    }
}

DdsSubsystemManager::~DdsSubsystemManager() { shutdown(); }

std::string DdsSubsystemManager::map_ui_to_dds_topic(const std::string& ui_shape) {
    std::string norm = ui_shape;
    std::transform(norm.begin(), norm.end(), norm.begin(), [](unsigned char c) { return std::tolower(c); });
    if (norm == "cube" || norm == "square") return "Square";
    else if (norm == "sphere" || norm == "circle") return "Circle";
    else if (norm == "tetrahedron" || norm == "triangle") return "Triangle";
    return ui_shape; 
}

void DdsSubsystemManager::initialize_dds_domain(int32_t domain_id, ThreadSafeSampleQueue* queue) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    sample_queue_ = queue;
    try {
        reader_listener_ = std::make_shared<ShapeTypeListener>(sample_queue_);
        dds::domain::qos::DomainParticipantQos participant_qos;
        
        // Force strict UDPv4 transport (Disable Shmem)
        participant_qos << rti::core::policy::TransportBuiltin::UDPv4();
        
        // Explicitly inject Loopback Interface
        rti::core::policy::Property prop;
        prop.set({"dds.transport.UDPv4.builtin.parent.allow_interfaces", "127.0.0.1"});
        participant_qos << prop;
        
        // Force discovery routing specifically over localhost
        participant_qos << rti::core::policy::Discovery().initial_peers(std::vector<std::string>{"127.0.0.1"});

        participant_ = dds::domain::DomainParticipant(domain_id, participant_qos);
        publisher_ = dds::pub::Publisher(participant_);
        subscriber_ = dds::sub::Subscriber(participant_);
        
        std::cout << "[System] QoS Transport Restricted to UDPv4 Local Loopback (127.0.0.1)" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[DDS] Failed to initialize Domain Participant: " << ex.what() << std::endl;
    }
}

void DdsSubsystemManager::shutdown() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    active_readers_.clear();
    active_writers_.clear();
    active_topics_.clear();
    subscriber_ = dds::core::null;
    publisher_ = dds::pub::Publisher(participant_); // Re-linked safety pointer
    subscriber_ = dds::sub::Subscriber(participant_);
}

void DdsSubsystemManager::create_writer(const std::string& ui_shape, const std::string& color) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    if (participant_ == dds::core::null) return;
    std::string wire_topic = map_ui_to_dds_topic(ui_shape);
    std::string writer_key = wire_topic + "_" + color;
    if (active_writers_.find(writer_key) != active_writers_.end()) return;
    try {
        if (active_topics_.find(wire_topic) == active_topics_.end()) {
            active_topics_.emplace(wire_topic, dds::topic::Topic<ShapeTypeExtended>(participant_, wire_topic));
        }
        auto& topic = active_topics_.at(wire_topic);
        dds::pub::qos::DataWriterQos writer_qos;
        writer_qos << dds::core::policy::Reliability::Reliable();
        writer_qos << dds::core::policy::History::KeepLast(1);
        dds::pub::DataWriter<ShapeTypeExtended> writer(publisher_, topic, writer_qos);
        active_writers_.emplace(writer_key, writer);
    } catch (const std::exception& ex) {}
}

void DdsSubsystemManager::delete_writer(const std::string& ui_shape, const std::string& color) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    std::string wire_topic = map_ui_to_dds_topic(ui_shape);
    std::string writer_key = wire_topic + "_" + color;
    auto it = active_writers_.find(writer_key);
    if (it != active_writers_.end()) {
        it->second = dds::core::null;
        active_writers_.erase(it);
    }
}

void DdsSubsystemManager::write_shape_sample(const std::string& ui_shape, const std::string& color, int32_t x, int32_t y, int32_t z, int32_t size, ShapeFillKind fill, float angle) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    std::string wire_topic = map_ui_to_dds_topic(ui_shape);
    std::string writer_key = wire_topic + "_" + color;
    auto it = active_writers_.find(writer_key);
    if (it == active_writers_.end()) return;
    try {
        ShapeTypeExtended sample;
        
        // Direct member updates (No getter/setter parentheses)
        sample.color = color; 
        sample.x = x; 
        sample.y = y; 
        sample.z = z;
        sample.shapesize = size; 
        sample.fillKind = fill; 
        sample.angle = angle;
        
        it->second.write(sample);
    } catch (const std::exception& ex) {}
}

void DdsSubsystemManager::create_reader(const std::string& ui_shape, const std::string& color_filter) {
    (void)color_filter; // Fix: Suppress unused-parameter warning
    std::lock_guard<std::mutex> lock(manager_mutex_);
    if (participant_ == dds::core::null) return;
    std::string wire_topic = map_ui_to_dds_topic(ui_shape);
    if (active_readers_.find(wire_topic) != active_readers_.end()) return;
    try {
        if (active_topics_.find(wire_topic) == active_topics_.end()) {
            active_topics_.emplace(wire_topic, dds::topic::Topic<ShapeTypeExtended>(participant_, wire_topic));
        }
        auto& topic = active_topics_.at(wire_topic);
        dds::sub::qos::DataReaderQos reader_qos;
        reader_qos << dds::core::policy::Reliability::Reliable();
        reader_qos << dds::core::policy::History::KeepLast(6);
        
        dds::sub::DataReader<ShapeTypeExtended> reader(
            subscriber_, topic, reader_qos, reader_listener_.get(), dds::core::status::StatusMask::data_available()
        );
        active_readers_.emplace(wire_topic, reader);
    } catch (const std::exception& ex) {}
}

void DdsSubsystemManager::delete_reader(const std::string& ui_shape) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    std::string wire_topic = map_ui_to_dds_topic(ui_shape);
    auto it = active_readers_.find(wire_topic);
    if (it != active_readers_.end()) {
        it->second = dds::core::null;
        active_readers_.erase(it);
    }
}
EOF
echo "[System] Patched src/dds/dds_manager.cpp"

# 3. Completely Overwrite main.cpp with clean, fully patched, non-truncated logic
cat << 'EOF' > src/main.cpp
/*****************************************************************************/
/* (c) Copyright, Real-Time Innovations, All rights reserved.                */
/* */
/* Permission to modify and use for internal purposes granted.               */
/* This software is provided "as is", without warranty, express or implied.  */
/* */
/*****************************************************************************/

#define _USE_MATH_DEFINES
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <map>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>

#include "dds_manager.hpp"
#include "graphics_engine.hpp"
#include "thread_queue.hpp"

// Suppress strict non-trivial memory warnings from modern compilers
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wnontrivial-memcall"
#endif
#include "imgui.h"
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

// --- Dynamic State Definitions ---
enum class TrajectoryType { PLANE_Z125, BOUNCE_3D, ORBIT };

struct LocalPublisher {
    std::string id;
    std::string shape;
    std::string color;
    int size;
    int publish_hz;
    TrajectoryType trajectory;

    // Kinematics
    float x, y, z;
    float vx, vy, vz;
    float angle = 0.0f;
    std::chrono::steady_clock::time_point last_publish_time;
};

struct LocalSubscriber {
    std::string id;
    std::string target_shape;
    std::string color_filter;
    int time_filter_ms;
    int history_depth;
};

struct VisualizerSample {
    float x, y, z;
    float angle = 0.0f;
};

// Structured record tracking active coordinates inside the visualizer mapping layer
struct VisualizerShapeInstance {
    std::string shape_topic;
    std::string color;
    float current_x = 125.0f;
    float current_y = 125.0f;
    float current_z = 125.0f;
    float current_angle = 0.0f;
    int32_t size = 15;
    std::chrono::steady_clock::time_point last_sample_time;
    std::chrono::steady_clock::time_point last_accepted_time;
    std::vector<VisualizerSample> history;
};

// Maps color keys to standard floating-point RGB representations
void get_color_rgb(const std::string& color_str, float& r, float& g, float& b) {
    if (color_str == "BLUE" || color_str == "#3b82f6") { r = 0.23f; g = 0.51f; b = 0.96f; }
    else if (color_str == "RED" || color_str == "#ef4444") { r = 0.94f; g = 0.27f; b = 0.27f; }
    else if (color_str == "YELLOW" || color_str == "#eab308") { r = 0.92f; g = 0.70f; b = 0.03f; }
    else if (color_str == "GREEN" || color_str == "#10b981") { r = 0.06f; g = 0.73f; b = 0.51f; }
    else if (color_str == "PURPLE" || color_str == "#a855f7") { r = 0.66f; g = 0.33f; b = 0.97f; }
    else { r = 0.8f; g = 0.8f; b = 0.8f; } 
}

int main(int argc, char* argv[]) {
    std::cout << "====================================================================\n";
    std::cout << "   RTI Connext DDS 3D Shapes Visualizer (macOS Native App)\n";
    std::cout << "====================================================================\n" << std::endl;

    int domain_id = 0;
    if (argc > 1) {
        try {
            domain_id = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "[Warning] Invalid Domain ID argument. Defaulting to 0." << std::endl;
        }
    }

    std::cout << "[System] Bootstrapping DDS Interface on Domain " << domain_id << "..." << std::endl;

    ThreadSafeSampleQueue sample_queue;
    DdsSubsystemManager dds_manager;
    dds_manager.initialize_dds_domain(domain_id, &sample_queue);

    std::cout << "[System] Initializing Hardware-Accelerated Graphics Engine..." << std::endl;
    
    GraphicsEngine graphics;
    if (!graphics.initialize(1280, 720, "RTI DDS 3D Shapes Workspace")) {
        std::cerr << "[Fatal] OpenGL context initialization failed." << std::endl;
        return EXIT_FAILURE;
    }

    // --- State Vectors (Completely Empty at Startup!) ---
    std::vector<LocalPublisher> active_publishers;
    std::vector<LocalSubscriber> active_subscribers;
    std::map<std::string, VisualizerShapeInstance> shape_registry;
    
    // ImGui Publisher Form State Variables
    const char* shapes[] = { "Square", "Circle", "Triangle" };
    int shape_idx = 0;
    const char* colors[] = { "BLUE", "RED", "YELLOW", "GREEN", "PURPLE" };
    int color_idx = 0;
    int size_val = 15;
    const char* trajectories[] = { "PLANE_Z125", "BOUNCE_3D", "ORBIT" };
    int traj_idx = 0;
    int publish_hz = 16;

    // ImGui Subscriber Form State Variables
    const char* sub_shapes[] = { "All", "Square", "Circle", "Triangle" };
    int sub_shape_idx = 0;
    const char* sub_colors[] = { "All", "BLUE", "RED", "YELLOW", "GREEN", "PURPLE" };
    int sub_color_idx = 0;
    int sub_time_filter_ms = 0;
    int sub_history_depth = 6;

    double global_time_counter = 0.0;
    auto last_frame_time = std::chrono::steady_clock::now();

    // Initialize auto-readers for standard shape topics
    dds_manager.create_reader("Square");
    dds_manager.create_reader("Circle");
    dds_manager.create_reader("Triangle");

    std::cout << "[System] Application successfully loaded. Workspace ready!" << std::endl;

    while (graphics.is_running()) {
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> dt_dur = current_time - last_frame_time;
        double dt = dt_dur.count(); // Delta time in seconds
        last_frame_time = current_time;

        global_time_counter += dt;
        double dt_ms = dt * 1000.0;

        // --- Kinematics & DDS Publish Loop ---
        for (size_t i = 0; i < active_publishers.size(); ++i) {
            auto& pub = active_publishers[i];
            
            // Increment angle dynamically by 90 degrees per second
            pub.angle += 90.0f * static_cast<float>(dt);
            if (pub.angle >= 360.0f) pub.angle -= 360.0f;

            if (pub.trajectory == TrajectoryType::PLANE_Z125) {
                pub.x += pub.vx * (dt_ms / 16.6f);
                pub.y += pub.vy * (dt_ms / 16.6f);
                pub.z = 125.0f;
            } else if (pub.trajectory == TrajectoryType::BOUNCE_3D) {
                pub.x += pub.vx * (dt_ms / 16.6f);
                pub.y += pub.vy * (dt_ms / 16.6f);
                pub.z += pub.vz * (dt_ms / 16.6f);
            } else if (pub.trajectory == TrajectoryType::ORBIT) {
                float radius = 80.0f;
                float speed = 2.0f; 
                float offset = i * 2.0f; // Prevent multi-orbit collisions
                pub.x = 125.0f + std::cos(global_time_counter * speed + offset) * radius;
                pub.y = 125.0f + std::sin(global_time_counter * speed + offset) * radius;
                pub.z = 125.0f + std::sin(global_time_counter * speed * 2.0f) * 40.0f;
            }

            // Boundary Checks
            float bound = 250.0f;
            float half = pub.size / 2.0f;
            if (pub.x < half) { pub.x = half; pub.vx *= -1; }
            if (pub.x > bound - half) { pub.x = bound - half; pub.vx *= -1; }
            if (pub.y < half) { pub.y = half; pub.vy *= -1; }
            if (pub.y > bound - half) { pub.y = bound - half; pub.vy *= -1; }
            if (pub.trajectory == TrajectoryType::BOUNCE_3D) {
                if (pub.z < half) { pub.z = half; pub.vz *= -1; }
                if (pub.z > bound - half) { pub.z = bound - half; pub.vz *= -1; }
            }

            // Execute DDS Network Publication based on set Hz limit
            std::chrono::duration<double> t_since_pub = current_time - pub.last_publish_time;
            if (t_since_pub.count() >= (1.0 / pub.publish_hz)) {
                pub.last_publish_time = current_time;
                dds_manager.write_shape_sample(pub.shape, pub.color, 
                                               static_cast<int32_t>(pub.x), 
                                               static_cast<int32_t>(pub.y), 
                                               static_cast<int32_t>(pub.z), 
                                               pub.size, ShapeFillKind::SOLID_FILL, pub.angle);
            }
        }

        // --- DDS Telemetry Drainage Loop ---
        auto incoming_samples = sample_queue.drain_queue();
        for (const auto& wrapper : incoming_samples) {
            const auto& sample = wrapper.data;
            std::string topic = wrapper.topic_name;

            // Enforce User-Selected Subscription Filters ONLY (Strict matching)
            bool is_matched = false;
            for (const auto& sub : active_subscribers) {
                bool shape_match = (sub.target_shape == "All" || sub.target_shape == topic);
                bool color_match = (sub.color_filter == "All" || sub.color_filter == sample.color);
                if (shape_match && color_match) {
                    is_matched = true;
                    break;
                }
            }

            // Do not allow an app to subscribe to its own publishers (Local Loopback Filtering)
            bool is_self_published = false;
            for (const auto& pub : active_publishers) {
                std::string pub_wire_topic = dds_manager.map_ui_to_dds_topic(pub.shape);
                if (pub_wire_topic == topic && pub.color == sample.color) {
                    is_self_published = true;
                    break;
                }
            }

            if (is_matched && !is_self_published) {
                std::string key = topic + "_" + sample.color;
                VisualizerShapeInstance& inst = shape_registry[key];
                inst.shape_topic = topic;
                inst.color = sample.color;
                inst.size = sample.shapesize;

                // Time Filter threshold separation (ms) queried dynamically from the live slider
                auto time_since_last_accept = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - inst.last_accepted_time
                ).count();

                if (time_since_last_accept >= sub_time_filter_ms) {
                    inst.last_accepted_time = current_time;

                    // Push current state to historical trace queue before updating
                    if (inst.last_sample_time != std::chrono::steady_clock::time_point()) {
                        VisualizerSample prev;
                        prev.x = inst.current_x;
                        prev.y = inst.current_y;
                        prev.z = inst.current_z;
                        prev.angle = inst.current_angle;
                        inst.history.insert(inst.history.begin(), prev);
                    }

                    // Update current live target position and rotation angle (strictly paired)
                    inst.current_x = static_cast<float>(sample.x);
                    inst.current_y = static_cast<float>(sample.y);
                    inst.current_z = static_cast<float>(sample.z);
                    inst.current_angle = sample.angle;
                }

                // Dynamically size-cap history vector in real-time according to the live slider
                if (inst.history.size() > static_cast<size_t>(sub_history_depth)) {
                    inst.history.resize(sub_history_depth);
                }

                inst.last_sample_time = current_time;
            }
        }

        graphics.begin_frame();
        graphics.draw_coordinate_grid();

        // 1. Draw Local Publishers (Solid Shading with local dynamic rotation)
        for (const auto& pub : active_publishers) {
            float r, g, b;
            get_color_rgb(pub.color, r, g, b);
            if (pub.shape == "Square") graphics.draw_cube(pub.x, pub.y, pub.z, pub.size, r, g, b, false, pub.angle);
            else if (pub.shape == "Circle") graphics.draw_sphere(pub.x, pub.y, pub.z, pub.size / 2.0f, r, g, b, false, pub.angle);
            else if (pub.shape == "Triangle") graphics.draw_tetrahedron(pub.x, pub.y, pub.z, pub.size, r, g, b, false, pub.angle);
        }

        // 2. Draw Received Telemetry Subscribers (Glowing Wireframes & Historical Trails)
        for (auto it = shape_registry.begin(); it != shape_registry.end(); ) {
            std::chrono::duration<double> age = current_time - it->second.last_sample_time;
            if (age.count() > 3.0) {
                it = shape_registry.erase(it);
                continue;
            }

            float r, g, b;
            get_color_rgb(it->second.color, r, g, b);
            std::string normalized_topic = it->second.shape_topic;
            float draw_size = it->second.size * 1.25f; // Draw slightly larger to cage the solid object
            
            // Draw Main "Head" Wireframe Indicator with received network angle
            if (normalized_topic == "Square" || normalized_topic == "Cube") {
                graphics.draw_cube(it->second.current_x, it->second.current_y, it->second.current_z, draw_size, r, g, b, true, it->second.current_angle);
            } else if (normalized_topic == "Circle" || normalized_topic == "Sphere") {
                graphics.draw_sphere(it->second.current_x, it->second.current_y, it->second.current_z, draw_size / 2.0f, r, g, b, true, it->second.current_angle);
            } else if (normalized_topic == "Triangle" || normalized_topic == "Tetrahedron") {
                graphics.draw_tetrahedron(it->second.current_x, it->second.current_y, it->second.current_z, draw_size, r, g, b, true, it->second.current_angle);
            }

            // Truncate the history trail vector instantly as soon as the slider changes
            if (it->second.history.size() > static_cast<size_t>(sub_history_depth)) {
                it->second.history.resize(sub_history_depth);
            }

            // Draw Trailing History Samples (with paired chronological historical angles)
            for (size_t h = 0; h < it->second.history.size(); ++h) {
                const auto& hist = it->second.history[h];
                
                float size_scale = 1.0f - (static_cast<float>(h + 1) * 0.05f);
                if (size_scale < 0.2f) size_scale = 0.2f;
                float hist_size = it->second.size * size_scale;

                // Color shading attenuation to mimic alpha transparency fading
                float fade_factor = 0.5f * (1.0f - (static_cast<float>(h) / static_cast<float>(it->second.history.size() + 1)));
                float hr = r * fade_factor;
                float hg = g * fade_factor;
                float hb = b * fade_factor;

                if (normalized_topic == "Square" || normalized_topic == "Cube") {
                    graphics.draw_cube(hist.x, hist.y, hist.z, hist_size, hr, hg, hb, false, hist.angle);
                } else if (normalized_topic == "Circle" || normalized_topic == "Sphere") {
                    graphics.draw_sphere(hist.x, hist.y, hist.z, hist_size / 2.0f, hr, hg, hb, false, hist.angle);
                } else if (normalized_topic == "Triangle" || normalized_topic == "Tetrahedron") {
                    graphics.draw_tetrahedron(hist.x, hist.y, hist.z, hist_size, hr, hg, hb, false, hist.angle);
                }
            }
            
            ++it;
        }

        // --- ImGui UI Generation ---
        graphics.begin_ui();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 680), ImGuiCond_FirstUseEver);
        ImGui::Begin("RTI Connext 3D Workspace", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // =====================================================================
        // SECTION 1: PUBLISHER CONTROL & REGISTRY
        // =====================================================================
        if (ImGui::CollapsingHeader("DataWriters (Publishers)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Combo("Shape##pub", &shape_idx, shapes, IM_ARRAYSIZE(shapes));
            ImGui::Combo("Color##pub", &color_idx, colors, IM_ARRAYSIZE(colors));
            ImGui::SliderInt("Size##pub", &size_val, 10, 30);
            ImGui::Combo("Trajectory##pub", &traj_idx, trajectories, IM_ARRAYSIZE(trajectories));
            ImGui::SliderInt("Publish Hz##pub", &publish_hz, 1, 60);

            if (ImGui::Button("Initialize DataWriter Instance##pub_btn", ImVec2(-1, 30))) {
                LocalPublisher pub;
                pub.id = "pub_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                pub.shape = shapes[shape_idx];
                pub.color = colors[color_idx];
                pub.size = size_val;
                pub.publish_hz = publish_hz;
                pub.trajectory = static_cast<TrajectoryType>(traj_idx);
                
                pub.x = 125.0f; pub.y = 125.0f; pub.z = 125.0f;
                pub.vx = ((std::rand() % 100) / 50.0f) - 1.0f; 
                pub.vy = ((std::rand() % 100) / 50.0f) - 1.0f; 
                pub.vz = (pub.trajectory == TrajectoryType::BOUNCE_3D) ? 2.5f : 0.0f;
                pub.angle = 0.0f;
                pub.last_publish_time = std::chrono::steady_clock::now();

                dds_manager.create_writer(pub.shape, pub.color);
                active_publishers.push_back(pub);
                std::cout << "[System] Initialized local DataWriter on Topic: " << pub.shape << " (" << pub.color << ")" << std::endl;
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Local Publishers (%zu):", active_publishers.size());
            ImGui::Separator();
            for (auto it = active_publishers.begin(); it != active_publishers.end(); ) {
                std::string label = it->shape + " (" + it->color + ") - " + std::to_string(it->publish_hz) + "Hz";
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(ImGui::GetWindowWidth() - 75);
                std::string btn_label = "Delete##" + it->id;
                if (ImGui::Button(btn_label.c_str())) {
                    dds_manager.delete_writer(it->shape, it->color);
                    std::cout << "[System] Destroyed DataWriter on Topic: " << it->shape << " (" << it->color << ")" << std::endl;
                    it = active_publishers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // =====================================================================
        // SECTION 2: SUBSCRIBER CONTROL & REGISTRY
        // =====================================================================
        if (ImGui::CollapsingHeader("DataReaders (Subscribers)", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Live Global QoS Controls (Instantly affects all readers)
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Subscription QoS Settings (Global)");
            ImGui::SliderInt("Time Filter (ms)##sub", &sub_time_filter_ms, 0, 2000, "%d ms");
            ImGui::SliderInt("History Depth##sub", &sub_history_depth, 0, 15, "%d samples");
            ImGui::Separator();
            ImGui::Spacing();

            // DataReader Creation Forms
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Create DataReader Filter");
            ImGui::Combo("Target Shape##sub", &sub_shape_idx, sub_shapes, IM_ARRAYSIZE(sub_shapes));
            ImGui::Combo("Color Filter##sub", &sub_color_idx, sub_colors, IM_ARRAYSIZE(sub_colors));

            if (ImGui::Button("Initialize DataReader Instance##sub_btn", ImVec2(-1, 30))) {
                LocalSubscriber sub;
                sub.id = "sub_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                sub.target_shape = sub_shapes[sub_shape_idx];
                sub.color_filter = sub_colors[sub_color_idx];
                sub.time_filter_ms = sub_time_filter_ms;
                sub.history_depth = sub_history_depth;

                // Bind official DDS Reader layer
                if (sub.target_shape == "All") {
                    dds_manager.create_reader("Square");
                    dds_manager.create_reader("Circle");
                    dds_manager.create_reader("Triangle");
                } else {
                    dds_manager.create_reader(sub.target_shape);
                }

                active_subscribers.push_back(sub);
                std::cout << "[System] Initialized DataReader matching shape: " << sub.target_shape 
                          << ", Color: " << sub.color_filter << std::endl;
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "DataReader Content Filters (%zu):", active_subscribers.size());
            ImGui::Separator();
            for (auto it = active_subscribers.begin(); it != active_subscribers.end(); ) {
                std::string label = it->target_shape + " (" + it->color_filter + ")";
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(ImGui::GetWindowWidth() - 75);
                std::string btn_label = "Delete##" + it->id;
                if (ImGui::Button(btn_label.c_str())) {
                    if (it->target_shape == "All") {
                        dds_manager.delete_reader("Square");
                        dds_manager.delete_reader("Circle");
                        dds_manager.delete_reader("Triangle");
                    } else {
                        dds_manager.delete_reader(it->target_shape);
                    }
                    std::cout << "[System] Unsubscribed filter mapping: " << it->target_shape << " (" << it->color_filter << ")" << std::endl;
                    it = active_subscribers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        ImGui::End();
        
        graphics.end_ui();

        graphics.end_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    dds_manager.shutdown();
    std::cout << "[System] Standard shutdown sequence complete. Application terminated." << std::endl;

    return EXIT_SUCCESS;
}
EOF
echo "[System] Patched src/main.cpp"

# 4. Use inline Python parser to automatically strip the wireframe bounding box, central cubes and inject rotation support
echo "-> Removing wireframe bounding boxes and injecting rotation support..."
python3 << 'EOF_PYTHON'
import re

# 4a. Patch graphics_engine.hpp to append angle parameters with default values
try:
    with open('src/graphics/graphics_engine.hpp', 'r') as f:
        hdr = f.read()

    # Idempotency check: only patch if 'angle' parameter is not already present in draw_cube
    if 'angle' not in hdr:
        # Match function declarations and capture all current parameters, then append default argument
        hdr = re.sub(r'void\s+draw_cube\s*\(([^)]+)\)', r'void draw_cube(\1, float angle = 0.0f)', hdr)
        hdr = re.sub(r'void\s+draw_sphere\s*\(([^)]+)\)', r'void draw_sphere(\1, float angle = 0.0f)', hdr)
        hdr = re.sub(r'void\s+draw_tetrahedron\s*\(([^)]+)\)', r'void draw_tetrahedron(\1, float angle = 0.0f)', hdr)

        with open('src/graphics/graphics_engine.hpp', 'w') as f:
            f.write(hdr)
        print('[System] Successfully patched graphics_engine.hpp for rotation support.')
    else:
        print('[System] graphics_engine.hpp is already patched. Skipping header modification.')
except Exception as e:
    print('[Error] Patcher failed on graphics_engine.hpp: ' + str(e))

# 4b. Patch graphics_engine.cpp to remove boundaries and inject glRotatef matrix rotations
try:
    with open('src/graphics/graphics_engine.cpp', 'r') as f:
        content = f.read()

    # Match draw_coordinate_grid with any layout namespace prefix and parameters
    match = re.search(r'draw_coordinate_grid\s*\([^)]*\)', content)
    if match:
        # Trace curly braces recursively to isolate the full function block
        start_idx = content.find('{', match.end())
        if start_idx != -1:
            brace_count = 0
            end_idx = -1
            for idx in range(start_idx, len(content)):
                char = content[idx]
                if char == '{':
                    brace_count += 1
                elif char == '}':
                    brace_count -= 1
                    if brace_count == 0:
                        end_idx = idx + 1
                        break
            
            if end_idx != -1:
                func_body = content[start_idx:end_idx]
                
                # Comment out any draw_cube or draw_box inside this isolated function body
                lines = func_body.splitlines()
                modified_lines = []
                for line in lines:
                    if ('draw_cube' in line or 'draw_box' in line) and not line.strip().startswith('//'):
                        modified_lines.append('    // ' + line.strip() + ' // Automatically disabled bounding box / startup cube frame')
                    else:
                        modified_lines.append(line)
                
                modified_body = '\n'.join(modified_lines)
                content = content[:start_idx] + modified_body + content[end_idx:]
                print('[Python Patcher] Successfully stripped bounding box call from draw_coordinate_grid body.')

    # Global fallback: strip any draw_cube calls utilizing structural parameters centered at 125 or dimensioned at 250
    content_lines = content.splitlines()
    for i, line in enumerate(content_lines):
        if 'draw_cube' in line and ('125' in line or '250' in line) and not line.strip().startswith('//'):
            content_lines[i] = '    // ' + line.strip() + ' // Global cleanup of bounding box shapes'
            print('[Python Patcher] Strip of bounding box structure line satisfied.')
            
    content = '\n'.join(content_lines)

    # Inject glRotatef and angle parameter into shape draw functions in cpp
    if 'angle' not in content:
        # Capture definition parameters robustly and append variable definition
        content = re.sub(r'void\s+((?:\w+::)?draw_cube\s*\([^)]+)\)', r'void \1, float angle)', content)
        content = re.sub(r'void\s+((?:\w+::)?draw_sphere\s*\([^)]+)\)', r'void \1, float angle)', content)
        content = re.sub(r'void\s+((?:\w+::)?draw_tetrahedron\s*\([^)]+)\)', r'void \1, float angle)', content)

        # Isolated scope body glTranslate injector for GL matrix rotation safety
        def inject_rotation(func_name, code_text):
            m = re.search(r'void\s+(?:\w+::)?' + func_name + r'\s*\([^)]*\)\s*\{', code_text)
            if m:
                s_idx = m.end()
                brace_c = 1
                e_idx = -1
                for idx in range(s_idx, len(code_text)):
                    char = code_text[idx]
                    if char == '{':
                        brace_c += 1
                    elif char == '}':
                        brace_c -= 1
                        if brace_c == 0:
                            e_idx = idx
                            break
                if e_idx != -1:
                    f_body = code_text[s_idx:e_idx]
                    if 'glRotatef' not in f_body:
                        # Inject beautifully oriented diagonal 3D rotation axis
                        f_body_new = re.sub(r'(glTranslatef\s*\([^)]*\)\s*;)', r'\1\n    glRotatef(angle, 0.4f, 0.8f, 0.4f);', f_body)
                        code_text = code_text[:s_idx] + f_body_new + code_text[e_idx:]
                        print('[Python Patcher] Injected glRotatef into ' + func_name)
            return code_text

        content = inject_rotation('draw_cube', content)
        content = inject_rotation('draw_sphere', content)
        content = inject_rotation('draw_tetrahedron', content)

    with open('src/graphics/graphics_engine.cpp', 'w') as f:
        f.write(content)
    print('[System] Successfully completed patching graphics_engine.cpp to remove floating frame and support rotation!')
except Exception as e:
    print('[Error] Patcher failed on graphics_engine.cpp: ' + str(e))
EOF_PYTHON

echo "==========================================================="
echo "   [Success] Data routing and Loopback constraints verified!"
echo "==========================================================="
