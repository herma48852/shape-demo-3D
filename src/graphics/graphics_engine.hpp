/*****************************************************************************/
/* (c) Copyright, Real-Time Innovations, All rights reserved.                */
/* */
/* Permission to modify and use for internal purposes granted.               */
/* This software is provided "as is", without warranty, express or implied.  */
/* */
/*****************************************************************************/

#pragma once
#include <string>

// Forward declaring GLFW window handles to avoid exposing headers everywhere
struct GLFWwindow;

// Update the private and public boundaries inside src/graphics/graphics_engine.hpp:
class GraphicsEngine {
public:
    GraphicsEngine();
    ~GraphicsEngine();

    bool initialize(int width, int height, const std::string& title);
    bool is_running() const;
    void begin_frame();
    void end_frame();

    // ImGui Context Hooks
    void begin_ui();
    void end_ui();

    void initialize_projection(int width, int height);
    void draw_cube(float x, float y, float z, float size, float r, float g, float b, bool wireframe = false, float angle = 0.0f);
    void draw_sphere(float x, float y, float z, float radius, float r, float g, float b, bool wireframe = false, float angle = 0.0f);
    void draw_tetrahedron(float x, float y, float z, float size, float r, float g, float b, bool wireframe = false, float angle = 0.0f);
    void draw_coordinate_grid();

    // Mouse orbital and panning coordinate handlers
    void handle_cursor_pos(double x, double y);
    void handle_mouse_button(int button, int action, int mods);
    void handle_scroll(double xoffset, double yoffset); // Added scroll support

    GLFWwindow* get_window() const { return window_; }

private:
    GLFWwindow* window_;
    bool is_initialized_;
    int width_;
    int height_;

    // Camera perspective transformation properties
    float rotation_x_;
    float rotation_y_;
    float zoom_scale_;         // Camera Zoom Tracker
    float pan_x_;              // Translation Panning X Tracker
    float pan_y_;              // Translation Panning Y Tracker
    
    bool mouse_pressed_;       // Left-Click status (Rotation)
    bool mouse_right_pressed_; // Right-Click status (Translation/Panning)
    double last_mouse_x_;
    double last_mouse_y_;
};
