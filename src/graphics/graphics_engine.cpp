/*****************************************************************************/
/* (c) Copyright, Real-Time Innovations, All rights reserved.                */
/* */
/* Permission to modify and use for internal purposes granted.               */
/* This software is provided "as is", without warranty, express or implied.  */
/* */
/*****************************************************************************/

#define _USE_MATH_DEFINES
#include <cmath>
#include "graphics_engine.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

// Suppress strict non-trivial memory warnings from modern compilers
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wnontrivial-memcall"
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

GraphicsEngine::GraphicsEngine() 
    : window_(nullptr), 
      is_initialized_(false),
      width_(1280),
      height_(720),
      rotation_x_(-20.0f),
      rotation_y_(45.0f),
      zoom_scale_(1.0f),        
      pan_x_(0.0f),             
      pan_y_(0.0f),
      mouse_pressed_(false),
      mouse_right_pressed_(false),
      last_mouse_x_(0.0),
      last_mouse_y_(0.0) {}

GraphicsEngine::~GraphicsEngine() {
    if (is_initialized_) {
        // Safely tear down ImGui context before window destruction
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

bool GraphicsEngine::initialize(int width, int height, const std::string& title) {
    width_ = width;
    height_ = height;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window_ = glfwCreateWindow(width_, height_, title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW Window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    
    // Bind input callbacks
    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(w));
        if (engine) engine->handle_cursor_pos(x, y);
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int mods) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(w));
        if (engine) engine->handle_mouse_button(button, action, mods);
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* w, double xoffset, double yoffset) {
        auto* engine = static_cast<GraphicsEngine*>(glfwGetWindowUserPointer(w));
        if (engine) engine->handle_scroll(xoffset, yoffset);
    });

    // --- ImGui Context Bootstrapping ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // PERMANENT FIX: Disable reading and writing of imgui.ini entirely.
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 120");
    // ---------------------------------------------

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH); 
    initialize_projection(width_, height_);

    is_initialized_ = true;
    return true;
}

bool GraphicsEngine::is_running() const {
    return is_initialized_ && !glfwWindowShouldClose(window_);
}

void GraphicsEngine::initialize_projection(int width, int height) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width_, height_);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    float frustum_size = 180.0f; 
    glOrtho(-frustum_size * aspect, frustum_size * aspect, 
            -frustum_size, frustum_size, 
            -1000.0, 1000.0);

    glMatrixMode(GL_MODELVIEW);
}

void GraphicsEngine::begin_frame() {
    glClearColor(0.08f, 0.12f, 0.20f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    glScalef(1.0f / zoom_scale_, 1.0f / zoom_scale_, 1.0f / zoom_scale_);
    glTranslatef(pan_x_, pan_y_, -300.0f);
    glRotatef(rotation_x_, 1.0f, 0.0f, 0.0f);
    glRotatef(rotation_y_, 0.0f, 1.0f, 0.0f);
    glTranslatef(-125.0f, -125.0f, -125.0f); 
}

void GraphicsEngine::begin_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GraphicsEngine::end_ui() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GraphicsEngine::end_frame() {
    if (window_) {
        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}

void GraphicsEngine::handle_cursor_pos(double x, double y) {
    float dx = static_cast<float>(x - last_mouse_x_);
    float dy = static_cast<float>(y - last_mouse_y_);

    last_mouse_x_ = x;
    last_mouse_y_ = y;

    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (mouse_pressed_) {
        rotation_y_ += dx * 0.4f;
        rotation_x_ += dy * 0.4f;
        if (rotation_x_ > 85.0f) rotation_x_ = 85.0f;
        if (rotation_x_ < -85.0f) rotation_x_ = -85.0f;
    } 
    else if (mouse_right_pressed_) {
        pan_x_ += dx * 0.4f * zoom_scale_;
        pan_y_ -= dy * 0.4f * zoom_scale_; 
    }
}

void GraphicsEngine::handle_mouse_button(int button, int action, int mods) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) mouse_pressed_ = true;
        else if (action == GLFW_RELEASE) mouse_pressed_ = false;
    } 
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) mouse_right_pressed_ = true;
        else if (action == GLFW_RELEASE) mouse_right_pressed_ = false;
    }

    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
        mouse_pressed_ = false;
        mouse_right_pressed_ = false;
    }
}

void GraphicsEngine::handle_scroll(double xoffset, double yoffset) {
    (void)xoffset;

    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    zoom_scale_ -= static_cast<float>(yoffset) * 0.06f;
    if (zoom_scale_ < 0.15f) zoom_scale_ = 0.15f;
    if (zoom_scale_ > 4.5f)  zoom_scale_ = 4.5f;
}

void GraphicsEngine::draw_coordinate_grid() {
    glColor3f(0.2f, 0.3f, 0.4f);
    glBegin(GL_LINES);
        glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(250.0f, 0.0f, 0.0f);
        glVertex3f(250.0f, 0.0f, 0.0f); glVertex3f(250.0f, 250.0f, 0.0f);
        glVertex3f(250.0f, 250.0f, 0.0f); glVertex3f(0.0f, 250.0f, 0.0f);
        glVertex3f(0.0f, 250.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f);

        glVertex3f(0.0f, 0.0f, 250.0f); glVertex3f(250.0f, 0.0f, 250.0f);
        glVertex3f(250.0f, 0.0f, 250.0f); glVertex3f(250.0f, 250.0f, 250.0f);
        glVertex3f(250.0f, 250.0f, 250.0f); glVertex3f(0.0f, 250.0f, 250.0f);
        glVertex3f(0.0f, 250.0f, 250.0f); glVertex3f(0.0f, 0.0f, 250.0f);

        glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 250.0f);
        glVertex3f(250.0f, 0.0f, 0.0f); glVertex3f(250.0f, 0.0f, 250.0f);
        glVertex3f(250.0f, 250.0f, 0.0f); glVertex3f(250.0f, 250.0f, 250.0f);
        glVertex3f(0.0f, 250.0f, 0.0f); glVertex3f(0.0f, 250.0f, 250.0f);
    glEnd();

    glColor4f(0.2f, 0.25f, 0.35f, 0.4f);
    glBegin(GL_LINES);
        for (int i = 0; i <= 250; i += 25) {
            glVertex3f(0.0f, static_cast<float>(i), 125.0f);
            glVertex3f(250.0f, static_cast<float>(i), 125.0f);
            glVertex3f(static_cast<float>(i), 0.0f, 125.0f);
            glVertex3f(static_cast<float>(i), 250.0f, 125.0f);
        }
    glEnd();
}

void GraphicsEngine::draw_cube(float x, float y, float z, float size, float r, float g, float b, bool wireframe, float angle, bool thin) {
    glPushMatrix();
    glTranslatef(x, y, z);
    
    if (thin) {
        glRotatef(angle, 0.0f, 0.0f, 1.0f); 
        glScalef(1.0f, 1.0f, 4.0f / size);       
    } else {
        glRotatef(angle, 0.4f, 0.8f, 0.4f); 
    }

    float h = size / 2.0f;

    if (wireframe) {
        glColor3f(r, g, b);
        glBegin(GL_LINES);
            glVertex3f(-h, -h,  h); glVertex3f( h, -h,  h);
            glVertex3f( h, -h,  h); glVertex3f( h,  h,  h);
            glVertex3f( h,  h,  h); glVertex3f(-h,  h,  h);
            glVertex3f(-h,  h,  h); glVertex3f(-h, -h,  h);
            glVertex3f(-h, -h, -h); glVertex3f( h, -h, -h);
            glVertex3f( h, -h, -h); glVertex3f( h,  h, -h);
            glVertex3f( h,  h, -h); glVertex3f(-h,  h, -h);
            glVertex3f(-h,  h, -h); glVertex3f(-h, -h, -h);
            glVertex3f(-h, -h,  h); glVertex3f(-h, -h, -h);
            glVertex3f( h, -h,  h); glVertex3f( h, -h, -h);
            glVertex3f( h,  h,  h); glVertex3f( h,  h, -h);
            glVertex3f(-h,  h,  h); glVertex3f(-h,  h, -h);
        glEnd();
    } else {
        glBegin(GL_QUADS);
            glColor3f(r * 1.0f, g * 1.0f, b * 1.0f);
            glVertex3f(-h, -h,  h); glVertex3f( h, -h,  h); glVertex3f( h,  h,  h); glVertex3f(-h,  h,  h);
            glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);
            glVertex3f(-h, -h, -h); glVertex3f(-h,  h, -h); glVertex3f( h,  h, -h); glVertex3f( h, -h, -h);
            glColor3f(r * 0.9f, g * 0.9f, b * 0.9f);
            glVertex3f(-h,  h, -h); glVertex3f(-h,  h,  h); glVertex3f( h,  h,  h); glVertex3f( h,  h, -h);
            glColor3f(r * 0.4f, g * 0.4f, b * 0.4f);
            glVertex3f(-h, -h, -h); glVertex3f( h, -h, -h); glVertex3f( h, -h,  h); glVertex3f(-h, -h,  h);
            glColor3f(r * 0.75f, g * 0.75f, b * 0.75f);
            glVertex3f( h, -h, -h); glVertex3f( h,  h, -h); glVertex3f( h,  h,  h); glVertex3f( h, -h,  h);
            glColor3f(r * 0.65f, g * 0.65f, b * 0.65f);
            glVertex3f(-h, -h, -h); glVertex3f(-h, -h,  h); glVertex3f(-h,  h,  h); glVertex3f(-h,  h, -h);
        glEnd();
    }
    glPopMatrix();
}

void GraphicsEngine::draw_sphere(float x, float y, float z, float radius, float r, float g, float b, bool wireframe, float angle, bool thin) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);

    if (thin) {
        // 2D Publisher Mapping: Render as an extruded flat disk cylinder instead of a squashed sphere
        float thickness = 4.0f;
        int segments = 24;

        if (wireframe) {
            glColor3f(r, g, b);
            // Draw Top Circle
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segments; ++i) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                glVertex3f(radius * cos(theta), radius * sin(theta), thickness / 2.0f);
            }
            glEnd();
            // Draw Bottom Circle
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < segments; ++i) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                glVertex3f(radius * cos(theta), radius * sin(theta), -thickness / 2.0f);
            }
            glEnd();
            // Draw Vertical Connecting Edges
            glBegin(GL_LINES);
            for (int i = 0; i < segments; i += 4) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                float cx = radius * cos(theta);
                float cy = radius * sin(theta);
                glVertex3f(cx, cy, thickness / 2.0f);
                glVertex3f(cx, cy, -thickness / 2.0f);
            }
            glEnd();
        } else {
            // Render Top Cap (Brightest)
            glColor3f(r * 1.0f, g * 1.0f, b * 1.0f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, thickness / 2.0f);
            for (int i = 0; i <= segments; ++i) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                glVertex3f(radius * cos(theta), radius * sin(theta), thickness / 2.0f);
            }
            glEnd();

            // Render Bottom Cap (Darkest)
            glColor3f(r * 0.4f, g * 0.4f, b * 0.4f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, -thickness / 2.0f);
            for (int i = segments; i >= 0; --i) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                glVertex3f(radius * cos(theta), radius * sin(theta), -thickness / 2.0f);
            }
            glEnd();

            // Render Extruded Rim Walls with Shading Match
            glBegin(GL_QUAD_STRIP);
            for (int i = 0; i <= segments; ++i) {
                float theta = 2.0f * M_PI * float(i) / float(segments);
                float cx = cos(theta);
                float cy = sin(theta);
                // Simple rim shading based on normal direction
                float shade = 0.6f + 0.3f * (cx * 0.707f + cy * 0.707f);
                glColor3f(r * shade, g * shade, b * shade);
                glVertex3f(radius * cx, radius * cy, thickness / 2.0f);
                glVertex3f(radius * cx, radius * cy, -thickness / 2.0f);
            }
            glEnd();
        }
    } else {
        // Standard 3D Sphere Pipeline
        glRotatef(angle, 0.4f, 0.8f, 0.4f); 
        int lats = 12, longs = 12;

        for (int i = 0; i <= lats; i++) {
            double lat0 = M_PI * (-0.5 + static_cast<double>(i - 1) / lats);
            double z0 = sin(lat0), r0 = cos(lat0);
            double lat1 = M_PI * (-0.5 + static_cast<double>(i) / lats);
            double z1 = sin(lat1), r1 = cos(lat1);

            if (wireframe) {
                glColor3f(r, g, b);
                glBegin(GL_LINE_LOOP);
            } else {
                glBegin(GL_QUAD_STRIP);
            }

            for (int j = 0; j <= longs; j++) {
                double lng = 2.0 * M_PI * static_cast<double>(j) / longs;
                double x_coord = cos(lng), y_coord = sin(lng);
                
                if (!wireframe) {
                    float nx0 = static_cast<float>(x_coord * r0);
                    float ny0 = static_cast<float>(y_coord * r0);
                    float nz0 = static_cast<float>(z0);
                    float dot0 = nx0 * 0.577f + ny0 * 0.577f + nz0 * 0.577f;
                    float shade0 = 0.65f + 0.35f * dot0;
                    glColor3f(r * shade0, g * shade0, b * shade0);
                }
                glVertex3f(static_cast<float>(x_coord * r0 * radius), static_cast<float>(y_coord * r0 * radius), static_cast<float>(z0 * radius));

                if (!wireframe) {
                    float nx1 = static_cast<float>(x_coord * r1);
                    float ny1 = static_cast<float>(y_coord * r1);
                    float nz1 = static_cast<float>(z1);
                    float dot1 = nx1 * 0.577f + ny1 * 0.577f + nz1 * 0.577f;
                    float shade1 = 0.65f + 0.35f * dot1;
                    glColor3f(r * shade1, g * shade1, b * shade1);
                }
                glVertex3f(static_cast<float>(x_coord * r1 * radius), static_cast<float>(y_coord * r1 * radius), static_cast<float>(z1 * radius));
            }
            glEnd();
        }
    }
    glPopMatrix();
}

void GraphicsEngine::draw_tetrahedron(float x, float y, float z, float size, float r, float g, float b, bool wireframe, float angle, bool thin) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);

    if (thin) {
        // 2D Publisher Mapping: Render as an extruded Triangular Prism instead of a squashed pyramid
        float thickness = 4.0f;
        float half_t = thickness / 2.0f;
        float rad = size * 0.5f;

        // Symmetric Equilateral Triangle coordinates aligned with the base space
        float tx0 = 0.0f,         ty0 = rad;
        float tx1 = -rad * 0.866f, ty1 = -rad * 0.5f;
        float tx2 = rad * 0.866f,  ty2 = -rad * 0.5f;

        if (wireframe) {
            glColor3f(r, g, b);
            // Top Cap Wireframe
            glBegin(GL_LINE_LOOP);
                glVertex3f(tx0, ty0, half_t); glVertex3f(tx1, ty1, half_t); glVertex3f(tx2, ty2, half_t);
            glEnd();
            // Bottom Cap Wireframe
            glBegin(GL_LINE_LOOP);
                glVertex3f(tx0, ty0, -half_t); glVertex3f(tx1, ty1, -half_t); glVertex3f(tx2, ty2, -half_t);
            glEnd();
            // Link pillars
            glBegin(GL_LINES);
                glVertex3f(tx0, ty0, half_t); glVertex3f(tx0, ty0, -half_t);
                glVertex3f(tx1, ty1, half_t); glVertex3f(tx1, ty1, -half_t);
                glVertex3f(tx2, ty2, half_t); glVertex3f(tx2, ty2, -half_t);
            glEnd();
        } else {
            // Render Top Face (Flat Equilateral Face)
            glColor3f(r * 1.0f, g * 1.0f, b * 1.0f);
            glBegin(GL_TRIANGLES);
                glVertex3f(tx0, ty0, half_t); glVertex3f(tx1, ty1, half_t); glVertex3f(tx2, ty2, half_t);
            glEnd();

            // Render Bottom Face (Darkest)
            glColor3f(r * 0.4f, g * 0.4f, b * 0.4f);
            glBegin(GL_TRIANGLES);
                glVertex3f(tx0, ty0, -half_t); glVertex3f(tx2, ty2, -half_t); glVertex3f(tx1, ty1, -half_t);
            glEnd();

            // Render Rectangular Side Wall Panels with distinct directional shading
            glBegin(GL_QUADS);
                // Side Panel 1
                glColor3f(r * 0.85f, g * 0.85f, b * 0.85f);
                glVertex3f(tx0, ty0, half_t); glVertex3f(tx1, ty1, half_t); glVertex3f(tx1, ty1, -half_t); glVertex3f(tx0, ty0, -half_t);
                // Side Panel 2
                glColor3f(r * 0.6f, g * 0.6f, b * 0.6f);
                glVertex3f(tx1, ty1, half_t); glVertex3f(tx2, ty2, half_t); glVertex3f(tx2, ty2, -half_t); glVertex3f(tx1, ty1, -half_t);
                // Side Panel 3
                glColor3f(r * 0.75f, g * 0.75f, b * 0.75f);
                glVertex3f(tx2, ty2, half_t); glVertex3f(tx0, ty0, half_t); glVertex3f(tx0, ty0, -half_t); glVertex3f(tx2, ty2, -half_t);
            glEnd();
        }
    } else {
        // Standard 3D Tetrahedron Pipeline
        glRotatef(angle, 0.4f, 0.8f, 0.4f); 
        float h = size * 0.816f, rad = size * 0.5f;

        float v0[3] = { 0.0f, h, 0.0f };
        float v1[3] = { -rad, 0.0f, rad };
        float v2[3] = { rad, 0.0f, rad };
        float v3[3] = { 0.0f, 0.0f, -rad * 1.5f };

        auto draw_face = [&](float* p1, float* p2, float* p3, float shade) {
            if (wireframe) {
                glColor3f(r, g, b);
                glBegin(GL_LINE_LOOP);
            } else {
                glColor3f(r * shade, g * shade, b * shade);
                glBegin(GL_TRIANGLES);
            }
            glVertex3fv(p1); glVertex3fv(p2); glVertex3fv(p3);
            glEnd();
        };

        draw_face(v0, v1, v2, 1.0f);
        draw_face(v0, v2, v3, 0.85f);
        draw_face(v0, v3, v1, 0.65f);
        draw_face(v1, v3, v2, 0.5f);
    }
    glPopMatrix();
}
