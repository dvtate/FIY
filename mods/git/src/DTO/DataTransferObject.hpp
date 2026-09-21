//
// Created by tate on 4/30/26.
//

#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

// TODO instead do this?: https://github.com/nlohmann/json#basic-usage
//  fuck exceptions

/// Generic interface for encoding DTOs for communication between peers
struct DataTransferObject {
    DataTransferObject() = default;
    virtual ~DataTransferObject() = default;

    /// What class of object does this DTO represent
    virtual std::string_view dto_class() = 0;

    /// Convert to a JSON representation for communication between peers
    virtual nlohmann::json to_json() {
        nlohmann::json json;
        json["class"] = dto_class();
        return json;
    };

    /// Update values
    virtual bool from_json(const nlohmann::json& json);

    bool json(const std::string_view json) {
        try {
            return from_json(nlohmann::json::parse(json));
        } catch (nlohmann::json::exception& e) {
            DEBUG_LOG("JSON error: " << e.what());
            return false;
        }
    }

    std::string json() {
        try {
            return to_json().dump();
        } catch (nlohmann::json::exception& e) {
            DEBUG_LOG("JSON error: " << e.what());
            throw e;
        }
    }
};
