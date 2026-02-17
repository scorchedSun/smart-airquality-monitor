#pragma once

#include "Component.h"

namespace ha {

class Sensor : public Component {
private:
    const std::string device_class_;
    const std::string unit_of_measurement_;
    const std::string value_template_;
    const std::string entity_category_;
    const std::string icon_;
    std::string manual_state_;
    
public:
    Sensor(const Device& device,
             std::string_view object_id,
             std::string_view friendly_name,
             std::string_view device_class_name,
             std::string_view unit,
             std::string_view discovery_prefix = "homeassistant",
             std::string_view category = "",
             std::string_view icon_name = "")
        : Component(device, "sensor", std::string(object_id), std::string(friendly_name), std::string(discovery_prefix))
        , device_class_(device_class_name)
        , unit_of_measurement_(unit)
        , value_template_(std::string("{{ value_json.").append(std::string(object_id)).append(" }}"))
        , entity_category_(category)
        , icon_(icon_name)
    {
    }

    void updateState(const std::string& state) {
        manual_state_ = state;
    }

    std::string getStatePayload() const override {
        return manual_state_;
    }

    DynamicJsonDocument getDiscoveryPayload(const Device& device) const override {
        DynamicJsonDocument doc = Component::getDiscoveryPayload(device);
        if (!device_class_.empty()) doc["dev_cla"] = device_class_;
        doc["val_tpl"] = value_template_;
        if (!unit_of_measurement_.empty()) doc["unit_of_meas"] = unit_of_measurement_;
        if (!entity_category_.empty()) doc["ent_cat"] = entity_category_;
        if (!icon_.empty()) doc["icon"] = icon_;
        return doc;
    }

    std::string getDeviceClass() const {
        return device_class_;
    }
};

} // namespace ha
