#pragma once

#include "HAComponent.h"

namespace ha {

class Sensor : public Component {
private:
    const std::string device_class;
    const std::string unit_of_measurement;
    const std::string value_template;
    const std::string entity_category;
    const std::string icon;
    std::string manual_state;
    
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
        , device_class(device_class_name)
        , unit_of_measurement(unit)
        , value_template(std::string("{{ value_json.").append(std::string(object_id)).append(" }}"))
        , entity_category(category)
        , icon(icon_name)
    {
    }

    void update_state(const std::string& state) {
        manual_state = state;
    }

    std::string get_state_payload() const override {
        return manual_state;
    }

    DynamicJsonDocument get_discovery_payload(const Device& device) const override {
        DynamicJsonDocument doc = Component::get_discovery_payload(device);
        if (!device_class.empty()) doc["dev_cla"] = device_class;
        doc["val_tpl"] = value_template;
        if (!unit_of_measurement.empty()) doc["unit_of_meas"] = unit_of_measurement;
        if (!entity_category.empty()) doc["ent_cat"] = entity_category;
        if (!icon.empty()) doc["icon"] = icon;
        return doc;
    }

    std::string get_device_class() const {
        return device_class;
    }
};

} // namespace ha
