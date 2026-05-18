/*****************************************************************************/
/* (c) Copyright, Real-Time Innovations, All rights reserved.                */
/* */
/* Permission to modify and use for internal purposes granted.               */
/* This software is provided "as is", without warranty, express or implied.  */
/* */
/*****************************************************************************/

#pragma once

#include <dds/dds.hpp>
#include <string>
#include <mutex>
#include <map>
#include <memory>
#include "ShapeType.hpp"
#include "thread_queue.hpp"

/**
 * @brief An asynchronous listener that intercepts incoming ShapeTypeExtended
 * samples from the Connext middleware and handles 2D to 3D coordinate synthesis.
 */
class ShapeTypeListener : public dds::sub::NoOpDataReaderListener<ShapeTypeExtended> {
private:
    ThreadSafeSampleQueue* target_queue_;

public:
    explicit ShapeTypeListener(ThreadSafeSampleQueue* queue);
    virtual ~ShapeTypeListener() = default;

    /**
     * @brief Triggered automatically by Connext middleware threads when new 
     * data arrives on the network interface.
     */
    void on_data_available(dds::sub::DataReader<ShapeTypeExtended>& reader) override;
};

/**
 * @brief Manages the initialization of the DomainParticipant, Topics, 
 * Publishers, Subscribers, and the lifecycles of active DataWriters and DataReaders.
 */
class DdsSubsystemManager {
private:
    std::mutex manager_mutex_;
    ThreadSafeSampleQueue* sample_queue_ = nullptr;

    // Core DDS Infrastructure Entities
    dds::domain::DomainParticipant participant_{dds::core::null};
    dds::pub::Publisher publisher_{dds::core::null};
    dds::sub::Subscriber subscriber_{dds::core::null};

    // Shared Topic References (Square, Circle, Triangle)
    std::map<std::string, dds::topic::Topic<ShapeTypeExtended>> active_topics_;

    // Active Writers and Readers Mapped by [TopicName_Color] & [TopicName] keys
    std::map<std::string, dds::pub::DataWriter<ShapeTypeExtended>> active_writers_;
    std::map<std::string, dds::sub::DataReader<ShapeTypeExtended>> active_readers_;

    // Shared asynchronous listener instance
    std::shared_ptr<ShapeTypeListener> reader_listener_;

public:
    DdsSubsystemManager() = default;
    ~DdsSubsystemManager();

    // Prevent copies of active DDS network subsystems
    DdsSubsystemManager(const DdsSubsystemManager&) = delete;
    DdsSubsystemManager& operator=(const DdsSubsystemManager&) = delete;

    /**
     * @brief Maps general UI/Visualizer parameters to official RTI Topic names on-the-wire.
     */
    std::string map_ui_to_dds_topic(const std::string& ui_shape);

    /**
     * @brief Instantiates a DomainParticipant and sets up baseline Publisher/Subscriber groups.
     */
    void initialize_dds_domain(int32_t domain_id, ThreadSafeSampleQueue* queue);

    /**
     * @brief Closes and reclaims all underlying DDS writers, readers, and context allocations.
     */
    void shutdown();

    /**
     * @brief Creates a specialized DataWriter endpoint on the bus for a target shape and color key.
     */
    void create_writer(const std::string& ui_shape, const std::string& color);

    /**
     * @brief Reclaims a specific DataWriter.
     */
    void delete_writer(const std::string& ui_shape, const std::string& color);

    /**
     * @brief Serializes and writes a coordinate update payload sample onto the DDS network bus.
     */
    void write_shape_sample(const std::string& ui_shape, const std::string& color, 
                            int32_t x, int32_t y, int32_t z, int32_t size, 
                            ShapeFillKind fill, float angle);

    /**
     * @brief Spawns a localized DataReader with a reliable QoS profile to subscribe to shape updates.
     */
    void create_reader(const std::string& ui_shape, const std::string& color_filter = "all");

    /**
     * @brief Reclaims a subscription DataReader.
     */
    void delete_reader(const std::string& ui_shape);
};
