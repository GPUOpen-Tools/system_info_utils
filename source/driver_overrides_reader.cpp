//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Driver Overrides reader implementation
//=============================================================================

#include "driver_overrides_reader.h"

#include <memory>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "definitions.h"
#include "driver_overrides_definitions.h"

namespace driver_overrides_utils
{
    /// @brief Check if a node with the given name exists as a child of the provided parent node.
    /// @param [in] parent The parent JSON node whose children will be searched.
    /// @param [in] name The name of the child node to search for.
    /// @return True when a child JSON node with the given name exists, and false if it doesn't.
    static bool DoesNodeExist(const rapidjson::Value& parent, const char* name)
    {
        return parent.IsObject() && parent.HasMember(name);
    }

    /// @brief Gets or creates a member object in a JSON value.
    /// @param [in, out] parent The parent JSON object.
    /// @param [in] name The member name.
    /// @param [in, out] alloc The allocator.
    /// @return Reference to the member value.
    static rapidjson::Value& GetOrCreateMember(rapidjson::Value& parent, const char* name, rapidjson::Document::AllocatorType& alloc)
    {
        if (!parent.HasMember(name))
        {
            rapidjson::Value memberName(name, alloc);
            rapidjson::Value memberValue(rapidjson::kObjectType);
            parent.AddMember(memberName, memberValue, alloc);
        }
        return parent[name];
    }

    /// @brief Gets or creates a member array in a JSON value.
    /// @param [in, out] parent The parent JSON object.
    /// @param [in] name The member name.
    /// @param [in, out] alloc The allocator.
    /// @return Reference to the member array value.
    static rapidjson::Value& GetOrCreateArray(rapidjson::Value& parent, const char* name, rapidjson::Document::AllocatorType& alloc)
    {
        if (!parent.HasMember(name))
        {
            rapidjson::Value memberName(name, alloc);
            rapidjson::Value memberValue(rapidjson::kArrayType);
            parent.AddMember(memberName, memberValue, alloc);
        }
        return parent[name];
    }

    /// @brief The interface for parsers that process the Driver Override JSON chunk.
    class IDriverOverridesParser
    {
    public:
        /// @brief Constructor.
        IDriverOverridesParser() = default;

        /// @brief Destructor.
        virtual ~IDriverOverridesParser()
        {
        }

        /// @brief Process the Driver Overrides JSON node.
        /// @param [in] driver_overrides_json The parent JSON node containing Driver Override fields.
        /// @param [in, out] out_processed_json_text The processed JSON text.
        /// @return True if parsing was successful, false if it failed.
        virtual bool Process(const rapidjson::Value& driver_overrides_json, std::string& out_processed_json_text)
        {
            SYSTEM_INFO_UNUSED(driver_overrides_json);
            SYSTEM_INFO_UNUSED(out_processed_json_text);

            return false;
        }
    };

    /// @brief JSON parser V1 for Driver Overrides.
    class DriverOverridesParserV1 : public IDriverOverridesParser
    {
    public:
        /// @brief Constructor.
        DriverOverridesParserV1() = default;

        /// @brief Destructor.
        virtual ~DriverOverridesParserV1() = default;

        /// @brief Process the Driver Overrides JSON node.
        /// The output will contain only Driver Settings/Experiments that the user has modified.
        /// @param [in] driver_overrides_json The parent JSON node containing Driver Override fields.
        /// @param [in, out] out_processed_json_text The JSON text for the filtered Driver Overrides.
        /// @return True if parsing was successful, false if it failed.
        virtual bool Process(const rapidjson::Value& driver_overrides_json, std::string& out_processed_json_text)
        {
            bool result = false;

            processed_doc_.SetObject();

            if (DoesNodeExist(driver_overrides_json, kNodeStringIsDriverExperiments))
            {
                ParseIsDriverExperiments(driver_overrides_json[kNodeStringIsDriverExperiments]);
            }
            else
            {
                is_driver_experiments_ = false;
            }

            if (DoesNodeExist(driver_overrides_json, kNodeStringComponents))
            {
                result = ParseComponents(driver_overrides_json[kNodeStringComponents]);
            }

            if (processed_doc_.MemberCount() > 0)
            {
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                processed_doc_.Accept(writer);
                out_processed_json_text = std::string(buffer.GetString(), buffer.GetSize());
            }
            else
            {
                out_processed_json_text.clear();
            }

            return result;
        }

    protected:
        /// @brief Parse the "IsDriverExperiments" node.
        /// @param [in] driver_overrides_json The JSON node containing the "IsDriverExperiments" field.
        /// @return True if parsing was successful, false if it failed.
        bool ParseIsDriverExperiments(const rapidjson::Value& driver_overrides_json)
        {
            bool result = false;

            is_driver_experiments_ = driver_overrides_json.IsBool() ? driver_overrides_json.GetBool() : false;

            auto& alloc = processed_doc_.GetAllocator();
            if (processed_doc_.HasMember(kNodeStringIsDriverExperiments))
            {
                processed_doc_[kNodeStringIsDriverExperiments].SetBool(is_driver_experiments_);
            }
            else
            {
                rapidjson::Value expKey(kNodeStringIsDriverExperiments, alloc);
                rapidjson::Value expVal(is_driver_experiments_);
                processed_doc_.AddMember(expKey, expVal, alloc);
            }

            result = true;
            return result;
        }

        /// @brief Parse the "Components" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Components" array.
        /// @return True if parsing was successful, false if it failed.
        bool ParseComponents(const rapidjson::Value& driver_overrides_json)
        {
            bool result = false;

            if (!driver_overrides_json.IsArray())
            {
                return false;
            }

            if (driver_overrides_json.Empty())
            {
                result = true;
            }
            else
            {
                for (rapidjson::SizeType i = 0; i < driver_overrides_json.Size(); ++i)
                {
                    const auto& component_entry = driver_overrides_json[i];

                    if (DoesNodeExist(component_entry, kNodeStringComponent))
                    {
                        result = ParseComponent(component_entry[kNodeStringComponent]);
                        if (!result)
                        {
                            break;
                        }

                        if (DoesNodeExist(component_entry, kNodeStringStructures))
                        {
                            result = ParseStructures(component_entry[kNodeStringStructures]);
                            if (!result)
                            {
                                break;
                            }
                        }
                    }
                }
            }

            return result;
        }

        /// @brief Parse the "Component" node.  The Component name will be cached for use later.
        /// @param [in] driver_overrides_json The JSON node containing the "Component" field.
        /// @return True if parsing was successful, false if it failed.
        bool ParseComponent(const rapidjson::Value& driver_overrides_json)
        {
            if (driver_overrides_json.IsString())
            {
                current_component_name_ = driver_overrides_json.GetString();
            }
            else
            {
                current_component_name_.clear();
            }

            bool result = !current_component_name_.empty();

            return result;
        }

        /// @brief Parse the "Structures" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Structures" object.
        /// @return True if parsing was successful, false if it failed.
        bool ParseStructures(const rapidjson::Value& driver_overrides_json)
        {
            bool result = false;

            if (!driver_overrides_json.IsObject())
            {
                return false;
            }

            for (auto it = driver_overrides_json.MemberBegin(); it != driver_overrides_json.MemberEnd(); ++it)
            {
                current_structure_name_ = it->name.GetString();
                if (current_structure_name_.empty())
                {
                    current_structure_name_ = kDriverOverridesmiscellaneousStructure;
                }

                result = ParseStructure(it->value);
                if (!result)
                {
                    break;
                }
            }

            return result;
        }

        /// @brief Parse the "Structure" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Structure" array.
        /// @return True if parsing was successful, false if it failed.
        bool ParseStructure(const rapidjson::Value& driver_overrides_json)
        {
            bool result = true;

            if (!driver_overrides_json.IsArray())
            {
                return false;
            }

            for (rapidjson::SizeType i = 0; i < driver_overrides_json.Size(); ++i)
            {
                result = ParseSetting(driver_overrides_json[i]);
                if (!result)
                {
                    break;
                }
            }

            return result;
        }

        /// @brief Parse the "Setting" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Setting" array.
        /// @return True if parsing was successful, false if it failed.
        virtual bool ParseSetting(const rapidjson::Value& driver_overrides_json)
        {
            bool result = true;
            auto& alloc = processed_doc_.GetAllocator();

            if ((DoesNodeExist(driver_overrides_json, kNodeStringSupported)) && (!driver_overrides_json[kNodeStringSupported].GetBool()))
            {
                // Skip this setting if it's not supported.
                return true;
            }

            if (DoesNodeExist(driver_overrides_json, kNodeStringUserOverride) && DoesNodeExist(driver_overrides_json, kNodeStringCurrent))
            {
                if (driver_overrides_json[kNodeStringUserOverride] == driver_overrides_json[kNodeStringCurrent])
                {
                    rapidjson::Value json_settings_node(rapidjson::kObjectType);
                    rapidjson::Value valKey(kNodeStringValue, alloc);
                    rapidjson::Value valVal(driver_overrides_json[kNodeStringUserOverride], alloc);
                    json_settings_node.AddMember(valKey, valVal, alloc);
                    rapidjson::Value nameKey(kNodeStringSettingName, alloc);
                    rapidjson::Value nameVal(driver_overrides_json[kNodeStringSettingName], alloc);
                    json_settings_node.AddMember(nameKey, nameVal, alloc);
                    rapidjson::Value descKey(kNodeStringDescription, alloc);
                    rapidjson::Value descVal(driver_overrides_json[kNodeStringDescription], alloc);
                    json_settings_node.AddMember(descKey, descVal, alloc);

                    if (is_driver_experiments_)
                    {
                        auto& structures = GetOrCreateMember(processed_doc_, kNodeStringStructures, alloc);
                        auto& arr        = GetOrCreateArray(structures, current_structure_name_.c_str(), alloc);
                        arr.PushBack(json_settings_node, alloc);
                    }
                    else
                    {
                        auto& components     = GetOrCreateMember(processed_doc_, kNodeStringComponents, alloc);
                        auto& component      = GetOrCreateMember(components, current_component_name_.c_str(), alloc);
                        auto& structures     = GetOrCreateMember(component, kNodeStringStructures, alloc);
                        auto& arr            = GetOrCreateArray(structures, current_structure_name_.c_str(), alloc);
                        arr.PushBack(json_settings_node, alloc);
                    }
                }
            }
            else
            {
                result = false;
            }

            return result;
        }

    protected:
        bool               is_driver_experiments_ = false;
        std::string        current_component_name_;
        std::string        current_structure_name_;
        rapidjson::Document processed_doc_;
    };

    /// @brief Create a parser to parse a versioned chunk of Driver Overrides JSON data.
    /// @param [in] version_number The version number of the parser instance to create.
    /// @return A Driver Overrides JSON parser.
    static std::shared_ptr<IDriverOverridesParser> CreateDriverOverridesParser(const uint32_t version_number)
    {
        std::shared_ptr<IDriverOverridesParser> result = nullptr;

        switch (version_number)
        {
        case 1:
            // NOTE: Version 1 not supported.
            break;

        case 2:
        case 3:
            result = std::make_shared<DriverOverridesParserV1>();
            break;

        default:
            return nullptr;
            break;
        }

        return result;
    }

    /// @brief Process the Driver Overrides JSON node (the root node).
    /// @param [in] driver_overrides_node The parent JSON node containing Driver Overrides data.
    /// @param [in] version The version of the Driver Overrides JSON data.
    /// @param [in, out] out_processed_json_text The json string after processing the Driver Overrides data.
    /// @return True if parsing was successful, and false if it failed.
    static bool ProcessDriverOverridesNode(const rapidjson::Value& driver_overrides_node, std::uint32_t version, std::string& out_processed_json_text)
    {
        bool                                    result = false;
        std::shared_ptr<IDriverOverridesParser> parser = CreateDriverOverridesParser(version);
        assert(parser != nullptr);
        if (parser != nullptr)
        {
            result = parser->Process(driver_overrides_node, out_processed_json_text);
        }

        return result;
    }

    /// @brief Implementation of the Driver Overrides Reader class.
    bool DriverOverridesReader::Parse(const std::string& driver_overrides_json_text, std::uint32_t version, std::string& out_processed_json_text)
    {
        rapidjson::Document driver_overrides_json;
        driver_overrides_json.Parse(driver_overrides_json_text.c_str());

        if (driver_overrides_json.HasParseError() || !driver_overrides_json.IsObject())
        {
            return false;
        }

        // Process a Driver Overrides chunk of JSON. Presumably from an RDF file.
        return ProcessDriverOverridesNode(driver_overrides_json, version, out_processed_json_text);
    }

#ifdef DRIVER_OVERRIDES_ENABLE_RDF
#ifdef RDF_CXX_BINDINGS
    bool DriverOverridesReader::IsChunkPresent(rdf::ChunkFile& file)
    {
        bool result = false;

        if (file.ContainsChunk(kDriverOverridesChunkIdentifier))
        {
            result = true;
        }

        return result;
    }

    bool DriverOverridesReader::Parse(rdf::ChunkFile& file, std::string& out_processed_json_text)
    {
        bool result = false;
        out_processed_json_text.clear();

        if (IsChunkPresent(file))
        {
            // Check if the version is supported.
            auto version = file.GetChunkVersion(kDriverOverridesChunkIdentifier);
            if ((version >= kDriverOverridesChunkVersionMin) && (version <= kDriverOverridesChunkVersionMax))
            {
                // Get the size of the chunk.
                auto chunk_size = file.GetChunkDataSize(kDriverOverridesChunkIdentifier);

                char* buffer = new (std::nothrow) char[chunk_size + 1];
                if (buffer != nullptr)
                {
                    file.ReadChunkDataToBuffer(kDriverOverridesChunkIdentifier, buffer);
                    buffer[chunk_size] = '\0';

                    // Parse the JSON text.
                    result = Parse(buffer, version, out_processed_json_text);
                    delete[] buffer;
                }
            }
        }
        else
        {
            // This chunk is optional, so no error is returned if it's not present.
            result = true;
        }

        return result;
    }
#endif  // RDF_CXX_BINDINGS
    bool DriverOverridesReader::IsChunkPresent(rdfChunkFile* file)
    {
        bool result = false;

        if (file != nullptr)
        {
            int contains{};
            rdfChunkFileContainsChunk(file, kDriverOverridesChunkIdentifier, 0, &contains);
            if (contains)
            {
                result = true;
            }
        }

        return result;
    }

    bool DriverOverridesReader::Parse(rdfChunkFile* file, std::string& out_processed_json_text)
    {
        assert(file != nullptr);

        bool result = false;
        out_processed_json_text.clear();

        if (IsChunkPresent(file))
        {
            // Check if the version is supported.
            uint32_t version{};
            rdfChunkFileGetChunkVersion(file, kDriverOverridesChunkIdentifier, 0, &version);
            if ((version >= kDriverOverridesChunkVersionMin) && (version <= kDriverOverridesChunkVersionMax))
            {
                // Get the size of the chunk.
                int64_t chunk_size{};
                rdfChunkFileGetChunkDataSize(file, kDriverOverridesChunkIdentifier, 0, &chunk_size);

                char* buffer = new (std::nothrow) char[chunk_size + 1];
                if (buffer != nullptr)
                {
                    rdfChunkFileReadChunkData(file, kDriverOverridesChunkIdentifier, 0, buffer);
                    buffer[chunk_size] = '\0';

                    // Parse the JSON text.
                    result = Parse(buffer, version, out_processed_json_text);
                    delete[] buffer;
                }
            }
        }
        else
        {
            // This chunk is optional, so no error is returned if it's not present.
            result = true;
        }

        return result;
    }

#endif  // DRIVER_OVERRIDES_ENABLE_RDF
}  // namespace driver_overrides_utils
