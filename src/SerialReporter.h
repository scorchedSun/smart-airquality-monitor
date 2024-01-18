#pragma once

#include "ValueReporter.h"

#include "Arduino.h"

class SerialReporter : public ValueReporter {
    
    private:
        std::string source;        

    public:
        SerialReporter() = delete;

        SerialReporter(std::string source)
            : ValueReporter()
            , source(source) {
        }

        void report(const JsonDocument& data) const override {
            
            const std::string header = std::string("_____Data from ").append(source).append("_____");
            Serial.println(header.c_str());

            serializeJsonPretty(data, Serial);

            const size_t header_length = header.size();
            std::string footer;
            for (size_t i = 0; i < header_length; ++i)
                footer.append("_");
            Serial.println(footer.c_str());
        }
};