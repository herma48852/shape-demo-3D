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
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 120");
    // ---------------------------------------------

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
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

    // BULLETPROOF FIX: If ImGui wants the mouse, ignore all dragging for the 3D scene.
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
        glVertex3f(0.0f, 250.0f, 0.0f); glVertex3f(0.0f, 0.0f, 250.0f);

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

void GraphicsEngine::draw_cube(float x, float y, float z, float size, float r, float g, float b, bool wireframe, float angle) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(angle, 0.4f, 0.8f, 0.4f);
    glColor3f(r, g, b);
    float h = size / 2.0f;

    if (wireframe) {
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
            glVertex3f(-h, -h,  h); glVertex3f( h, -h,  h); glVertex3f( h,  h,  h); glVertex3f(-h,  h,  h);
            glVertex3f(-h, -h, -h); glVertex3f(-h,  h, -h); glVertex3f( h,  h, -h); glVertex3f( h, -h, -h);
            glVertex3f(-h,  h, -h); glVertex3f(-h,  h,  h); glVertex3f( h,  h,  h); glVertex3f( h,  h, -h);
            glVertex3f(-h, -h, -h); glVertex3f( h, -h, -h); glVertex3f( h, -h,  h); glVertex3f(-h, -h,  h);
            glVertex3f( h, -h, -h); glVertex3f( h,  h, -h); glVertex3f( h,  h,  h); glVertex3f( h, -h,  h);
            glVertex3f(-h, -h, -h); glVertex3f(-h, -h,  h); glVertex3f(-h,  h,  h); glVertex3f(-h,  h, -h);
        glEnd();
    }
    glPopMatrix();
}

void GraphicsEngine::draw_sphere(float x, float y, float z, float radius, float r, float g, float b, bool wireframe, float angle) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(angle, 0.4f, 0.8f, 0.4f);
    glColor3f(r, g, b);
    int lats = 12, longs = 12;

    for (int i = 0; i <= lats; i++) {
        double lat0 = M_PI * (-0.5 + static_cast<double>(i - 1) / lats);
        double z0 = sin(lat0), r0 = cos(lat0);
        double lat1 = M_PI * (-0.5 + static_cast<double>(i) / lats);
        double z1 = sin(lat1), r1 = cos(lat1);

        if (wireframe) glBegin(GL_LINE_LOOP);
        else glBegin(GL_QUAD_STRIP);

        for (int j = 0; j <= longs; j++) {
            double lng = 2.0 * M_PI * static_cast<double>(j) / longs;
            double x_coord = cos(lng), y_coord = sin(lng);
            glVertex3f(static_cast<float>(x_coord * r0 * radius), static_cast<float>(y_coord * r0 * radius), static_cast<float>(z0 * radius));
            glVertex3f(static_cast<float>(x_coord * r1 * radius), static_cast<float>(y_coord * r1 * radius), static_cast<float>(z1 * radius));
        }
        glEnd();
    }
    glPopMatrix();
}

void GraphicsEngine::draw_tetrahedron(float x, float y, float z, float size, float r, float g, float b, bool wireframe, float angle) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(angle, 0.4f, 0.8f, 0.4f);
    glColor3f(r, g, b);
    float h = size * 0.816f, rad = size * 0.5f;

    float v0[3] = { 0.0f, h, 0.0f };
    float v1[3] = { -rad, 0.0f, rad };
    float v2[3] = { rad, 0.0f, rad };
    float v3[3] = { 0.0f, 0.0f, -rad * 1.5f };

    auto draw_face = [&](float* p1, float* p2, float* p3) {
        if (wireframe) glBegin(GL_LINE_LOOP);
        else glBegin(GL_TRIANGLES);
        glVertex3fv(p1); glVertex3fv(p2); glVertex3fv(p3);
        glEnd();
    };

    draw_face(v0, v1, v2);
    draw_face(v0, v2, v3);
    draw_face(v0, v3, v1);
    draw_face(v1, v3, v2);
    glPopMatrix();
}