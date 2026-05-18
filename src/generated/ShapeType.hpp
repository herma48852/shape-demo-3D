

/*
WARNING: THIS FILE IS AUTO-GENERATED. DO NOT MODIFY.

This file was generated from ShapeType.idl
using RTI Code Generator (rtiddsgen) version 4.7.0.
The rtiddsgen tool is part of the RTI Connext DDS distribution.
For more information, type 'rtiddsgen -help' at a command shell
or consult the Code Generator User's Manual.
*/

#ifndef ShapeType_434673657_hpp
#define ShapeType_434673657_hpp

#include <iosfwd>

#if defined(NDDS_USER_DLL_EXPORT) && defined(RTI_WIN32)
#undef RTIUSERDllExport
#define RTIUSERDllExport __declspec(dllexport)
#endif

#if !defined(RTI_WIN32) && defined(NDDS_USER_SYMBOL_EXPORT)
#undef RTIUSERDllExport
#define RTIUSERDllExport __attribute__((visibility("default")))
#endif

#include "dds/core/SafeEnumeration.hpp"
#include "dds/core/String.hpp"
#include "dds/core/array.hpp"
#include "dds/core/vector.hpp"
#include "dds/core/External.hpp"
#include "rti/core/LongDouble.hpp"
#include "rti/core/Pointer.hpp"
#include "rti/core/array.hpp"
#include "rti/topic/TopicTraits.hpp"

#include "omg/types/string_view.hpp"

#include "omg/types/sequence.hpp"
#include "omg/types/optional.hpp"

#ifndef NDDS_STANDALONE_TYPE
#include "cdr/cdr_typeCode.h"
#include "dds/domain/DomainParticipant.hpp"
#include "dds/topic/TopicTraits.hpp"
#include "dds/core/xtypes/DynamicType.hpp"
#include "dds/core/xtypes/StructType.hpp"
#include "dds/core/xtypes/UnionType.hpp"
#include "dds/core/xtypes/EnumType.hpp"
#include "dds/core/xtypes/AliasType.hpp"
#include "rti/util/StreamFlagSaver.hpp"
#include "rti/domain/PluginSupport.hpp"
#endif

#if defined(NDDS_USER_DLL_EXPORT) || defined(NDDS_USER_SYMBOL_EXPORT)
#undef RTIUSERDllExport
#define RTIUSERDllExport
#endif

#if defined(NDDS_USER_DLL_EXPORT) && defined(RTI_WIN32)
#undef NDDSUSERDllExport
#define NDDSUSERDllExport __declspec(dllexport)
#endif

#if !defined(RTI_WIN32) && defined(NDDS_USER_SYMBOL_EXPORT)
#undef NDDSUSERDllExport
#define NDDSUSERDllExport __attribute__((visibility("default")))
#endif

enum class ShapeFillKind {
    SOLID_FILL, 
    TRANSPARENT_FILL, 
    HORIZONTAL_HATCH_FILL, 
    VERTICAL_HATCH_FILL
};

NDDSUSERDllExport std::ostream& operator << (std::ostream& o,const ShapeFillKind& sample);

struct NDDSUSERDllExport ShapeType {
    std::string color {};
    int32_t x {};
    int32_t y {};
    int32_t shapesize {};

    ShapeType();

    ShapeType(const ::omg::types::string_view& color_,int32_t x_,int32_t y_,int32_t shapesize_);

};

NDDSUSERDllExport bool operator == (const ShapeType& a, const ShapeType& b);
NDDSUSERDllExport bool operator != (const ShapeType& a, const ShapeType& b);
NDDSUSERDllExport void swap(ShapeType& a, ShapeType& b)  noexcept;

NDDSUSERDllExport std::ostream& operator<<(std::ostream& o, const ShapeType& sample);

struct NDDSUSERDllExport ShapeTypeExtended
: public ::ShapeType {
    ::ShapeFillKind fillKind {};
    float angle {};
    int32_t z {};

    ShapeTypeExtended();

    ShapeTypeExtended(const ::omg::types::string_view& color_,int32_t x_,int32_t y_,int32_t shapesize_,const ::ShapeFillKind& fillKind_,float angle_,int32_t z_);

};

NDDSUSERDllExport bool operator == (const ShapeTypeExtended& a, const ShapeTypeExtended& b);
NDDSUSERDllExport bool operator != (const ShapeTypeExtended& a, const ShapeTypeExtended& b);
NDDSUSERDllExport void swap(ShapeTypeExtended& a, ShapeTypeExtended& b)  noexcept;

NDDSUSERDllExport std::ostream& operator<<(std::ostream& o, const ShapeTypeExtended& sample);

#ifdef NDDS_STANDALONE_TYPE
namespace rti { 
    namespace topic {
        template <>
        struct default_enumerator< ::ShapeFillKind>
        {
            static const ::ShapeFillKind value;
        };
    }
}
#else

namespace rti {
    namespace flat {
        namespace topic {
        }
    }
}
namespace dds {
    namespace topic {

        template<>
        struct topic_type_name< ::ShapeType > {
            NDDSUSERDllExport static std::string value() {
                return "ShapeType";
            }
        };

        template<>
        struct is_topic_type< ::ShapeType > : public ::dds::core::true_type {};

        template<>
        struct topic_type_support< ::ShapeType > {
            NDDSUSERDllExport 
            static void register_type(
                ::dds::domain::DomainParticipant& participant,
                const std::string & type_name);

            NDDSUSERDllExport 
            static std::vector<char>& to_cdr_buffer(
                std::vector<char>& buffer, 
                const ::ShapeType& sample,
                ::dds::core::policy::DataRepresentationId representation 
                = ::dds::core::policy::DataRepresentation::auto_id());

            NDDSUSERDllExport 
            static void from_cdr_buffer(::ShapeType& sample, const std::vector<char>& buffer);
            NDDSUSERDllExport 
            static void reset_sample(::ShapeType& sample);

            NDDSUSERDllExport 
            static void allocate_sample(::ShapeType& sample, int, int);

            static const ::rti::topic::TypePluginKind::type type_plugin_kind = 
            ::rti::topic::TypePluginKind::STL;
        };
        template<>
        struct topic_type_name< ::ShapeTypeExtended > {
            NDDSUSERDllExport static std::string value() {
                return "ShapeTypeExtended";
            }
        };

        template<>
        struct is_topic_type< ::ShapeTypeExtended > : public ::dds::core::true_type {};

        template<>
        struct topic_type_support< ::ShapeTypeExtended > {
            NDDSUSERDllExport 
            static void register_type(
                ::dds::domain::DomainParticipant& participant,
                const std::string & type_name);

            NDDSUSERDllExport 
            static std::vector<char>& to_cdr_buffer(
                std::vector<char>& buffer, 
                const ::ShapeTypeExtended& sample,
                ::dds::core::policy::DataRepresentationId representation 
                = ::dds::core::policy::DataRepresentation::auto_id());

            NDDSUSERDllExport 
            static void from_cdr_buffer(::ShapeTypeExtended& sample, const std::vector<char>& buffer);
            NDDSUSERDllExport 
            static void reset_sample(::ShapeTypeExtended& sample);

            NDDSUSERDllExport 
            static void allocate_sample(::ShapeTypeExtended& sample, int, int);

            static const ::rti::topic::TypePluginKind::type type_plugin_kind = 
            ::rti::topic::TypePluginKind::STL;
        };
    }
}

namespace rti { 
    namespace topic {

        template <>
        struct default_enumerator< ::ShapeFillKind>
        {
            static const ::ShapeFillKind value;
        };
        template<>
        struct dynamic_type< ::ShapeFillKind > {
            typedef ::dds::core::xtypes::EnumType type;
            NDDSUSERDllExport static const ::dds::core::xtypes::EnumType& get();
        };

        template <>
        struct extensibility< ::ShapeFillKind > {
            static const ::dds::core::xtypes::ExtensibilityKind::type kind =
            ::dds::core::xtypes::ExtensibilityKind::EXTENSIBLE;    };

        template<>
        struct dynamic_type< ::ShapeType > {
            typedef ::dds::core::xtypes::StructType type;
            NDDSUSERDllExport static const ::dds::core::xtypes::StructType& get();
        };

        template <>
        struct extensibility< ::ShapeType > {
            static const ::dds::core::xtypes::ExtensibilityKind::type kind =
            ::dds::core::xtypes::ExtensibilityKind::EXTENSIBLE;    };

        template<>
        struct dynamic_type< ::ShapeTypeExtended > {
            typedef ::dds::core::xtypes::StructType type;
            NDDSUSERDllExport static const ::dds::core::xtypes::StructType& get();
        };

        template <>
        struct extensibility< ::ShapeTypeExtended > {
            static const ::dds::core::xtypes::ExtensibilityKind::type kind =
            ::dds::core::xtypes::ExtensibilityKind::EXTENSIBLE;    };

    }
}

#endif // NDDS_STANDALONE_TYPE
#if defined(NDDS_USER_DLL_EXPORT) || defined(NDDS_USER_SYMBOL_EXPORT)
#undef NDDSUSERDllExport
#define NDDSUSERDllExport
#endif

#endif // ShapeType_434673657_hpp
