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
