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
    bool is_2d = true; // Flag determining if publisher is 2D
};

// Maps color keys to standard floating-point RGB representations
void get_color_rgb(const std::string& color_str, float& r, float& g, float& b) {
    if (color_str == "BLUE" || color_str == "#3b82f6") { r = 0.23f; g = 0.51f; b = 0.96f; }
    else if (color_str == "RED" || color_str == "#ef4444") { r = 0.94f; g = 0.27f; b = 0.27f; }
    else if (color_str == "YELLOW" || color_str == "#eab308") { r = 0.92f; g = 0.70f; b = 0.03f; }
    else if (color_str == "GREEN" || color_str == "#10b981") { r = 0.06f; g = 0.73f; b = 0.51f; }
    else if (color_str == "PURPLE" || color_str == "#a855f7") { r = 0.66f; g = 0.33f; b = 0.97f; }
    else if (color_str == "CYAN") { r = 0.02f; g = 0.71f; b = 0.84f; }
    else if (color_str == "MAGENTA") { r = 0.93f; g = 0.25f; b = 0.68f; }
    else if (color_str == "ORANGE") { r = 0.96f; g = 0.58f; b = 0.11f; }
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
    if (!graphics.initialize(1280, 720, "3D Shapes Demo")) {
        std::cerr << "[Fatal] OpenGL context initialization failed." << std::endl;
        return EXIT_FAILURE;
    }

    // --- State Vectors ---
    std::vector<LocalPublisher> active_publishers;
    std::vector<LocalSubscriber> active_subscribers;
    std::map<std::string, VisualizerShapeInstance> shape_registry;
    
    // ImGui Publisher Form State Variables
    const char* shapes[] = { "Square", "Circle", "Triangle" };
    int shape_idx = 0;
    const char* colors[] = { "BLUE", "RED", "YELLOW", "GREEN", "PURPLE", "CYAN", "MAGENTA", "ORANGE" };
    int color_idx = 0;
    int size_val = 15;
    const char* trajectories[] = { "PLANE_Z125", "BOUNCE_3D", "ORBIT" };
    int traj_idx = 0;
    int publish_hz = 16;

    // ImGui Subscriber Form State Variables
    const char* sub_shapes[] = { "All", "Square", "Circle", "Triangle" };
    int sub_shape_idx = 0;
    const char* sub_colors[] = { "All", "BLUE", "RED", "YELLOW", "GREEN", "PURPLE", "CYAN", "MAGENTA", "ORANGE" };
    int sub_color_idx = 0;
    
    int sub_time_filter_ms = 0;
    int sub_history_depth = 6;

    double global_time_counter = 0.0;
    auto last_frame_time = std::chrono::steady_clock::now();

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

            // Resolve target active subscriber matching this incoming stream
            const LocalSubscriber* matching_sub = nullptr;
            for (const auto& sub : active_subscribers) {
                bool shape_match = (sub.target_shape == "All" || sub.target_shape == topic);
                bool color_match = (sub.color_filter == "All" || sub.color_filter == sample.color);
                if (shape_match && color_match) {
                    matching_sub = &sub;
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

            // Apply telemetry filter only using the captured QoS values of the initialized subscription
            if (matching_sub != nullptr && !is_self_published) {
                std::string key = topic + "_" + sample.color;
                VisualizerShapeInstance& inst = shape_registry[key];

                // --- GHOST & DUPLICATE SAMPLE AVOIDANCE ---
                // Since VOLATILE durability doesn't burst, history compression upon reset is 
                // caused by ghost readers (duplicate DataReaders on the same topic) pushing 
                // redundant samples into the queue in the exact same frame. 
                // This block catches and ignores those identical duplicate samples.
                if (inst.last_sample_time != std::chrono::steady_clock::time_point() &&
                    static_cast<float>(sample.x) == inst.current_x && 
                    static_cast<float>(sample.y) == inst.current_y && 
                    static_cast<float>(sample.z) == inst.current_z && 
                    sample.angle == inst.current_angle) {
                    continue; 
                }

                inst.shape_topic = topic;
                inst.color = sample.color;
                inst.size = sample.shapesize;

                // Identify if the incoming source is publishing 3D coordinates (not 2D z=125)
                if (sample.z != 125) {
                    inst.is_2d = false;
                }

                // Query captured parameters matching this active subscriber
                int active_time_filter_ms = matching_sub->time_filter_ms;
                int active_history_depth = matching_sub->history_depth;

                auto time_since_last_accept = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - inst.last_accepted_time
                ).count();

                if (time_since_last_accept >= active_time_filter_ms) {
                    inst.last_accepted_time = current_time;

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

                // Dynamically size-cap history vector in real-time according to the active subscription depth
                if (inst.history.size() > static_cast<size_t>(active_history_depth)) {
                    inst.history.resize(active_history_depth);
                }

                inst.last_sample_time = current_time;
            }
        }

        graphics.begin_frame();
        graphics.draw_coordinate_grid();

        // 1. Draw Local Publishers (Solid Shading with local dynamic rotation and thickness logic)
        for (const auto& pub : active_publishers) {
            float r, g, b;
            get_color_rgb(pub.color, r, g, b);
            bool thin = (pub.trajectory == TrajectoryType::PLANE_Z125);
            if (pub.shape == "Square") graphics.draw_cube(pub.x, pub.y, pub.z, pub.size, r, g, b, false, pub.angle, thin);
            else if (pub.shape == "Circle") graphics.draw_sphere(pub.x, pub.y, pub.z, pub.size / 2.0f, r, g, b, false, pub.angle, thin);
            else if (pub.shape == "Triangle") graphics.draw_tetrahedron(pub.x, pub.y, pub.z, pub.size, r, g, b, false, pub.angle, thin);
        }

        // 2. Draw Received Telemetry Subscribers (Glowing Wireframes & Historical Trails with thickness logic)
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
            bool thin = it->second.is_2d; // Thin if subscribed to a 2D publisher
            
            // Resolve active history depth value configured for this visualizer shape topic
            int active_history_depth = 6; 
            for (const auto& sub : active_subscribers) {
                bool shape_match = (sub.target_shape == "All" || sub.target_shape == normalized_topic);
                bool color_match = (sub.color_filter == "All" || sub.color_filter == it->second.color);
                if (shape_match && color_match) {
                    active_history_depth = sub.history_depth;
                    break;
                }
            }

            // Draw Main "Head" Wireframe Indicator with received network angle
            if (normalized_topic == "Square" || normalized_topic == "Cube") {
                graphics.draw_cube(it->second.current_x, it->second.current_y, it->second.current_z, draw_size, r, g, b, true, it->second.current_angle, thin);
            } else if (normalized_topic == "Circle" || normalized_topic == "Sphere") {
                graphics.draw_sphere(it->second.current_x, it->second.current_y, it->second.current_z, draw_size / 2.0f, r, g, b, true, it->second.current_angle, thin);
            } else if (normalized_topic == "Triangle" || normalized_topic == "Tetrahedron") {
                graphics.draw_tetrahedron(it->second.current_x, it->second.current_y, it->second.current_z, draw_size, r, g, b, true, it->second.current_angle, thin);
            }

            // Truncate the history trail vector instantly as soon as the active subscriber value says so
            if (it->second.history.size() > static_cast<size_t>(active_history_depth)) {
                it->second.history.resize(active_history_depth);
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
                    graphics.draw_cube(hist.x, hist.y, hist.z, hist_size, hr, hg, hb, false, hist.angle, thin);
                } else if (normalized_topic == "Circle" || normalized_topic == "Sphere") {
                    graphics.draw_sphere(hist.x, hist.y, hist.z, hist_size / 2.0f, hr, hg, hb, false, hist.angle, thin);
                } else if (normalized_topic == "Triangle" || normalized_topic == "Tetrahedron") {
                    graphics.draw_tetrahedron(hist.x, hist.y, hist.z, hist_size, hr, hg, hb, false, hist.angle, thin);
                }
            }
            
            ++it;
        }

        // --- ImGui UI Generation ---
        graphics.begin_ui();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 680), ImGuiCond_FirstUseEver);
        ImGui::Begin("3D Shapes Demo", nullptr, ImGuiWindowFlags_NoCollapse);
        
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
                
                // Flush the old rendering history cache when creating a new reader
                shape_registry.clear();
                
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
                    
                    // Flush the rendering history cache when deleting a reader
                    shape_registry.clear();
                    
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
