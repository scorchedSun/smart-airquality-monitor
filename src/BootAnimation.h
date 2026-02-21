#pragma once

#include <atomic>
#include <Arduino.h>
#include "Display.h"

class BootAnimation {
public:
    explicit BootAnimation(Display& display)
        : display_(display) {}

    void start() {
        active_.store(true);
        xTaskCreatePinnedToCore(taskFunc, "BootAnim", 2048, this, 1, nullptr, 0);
    }

    void stop() {
        active_.store(false);
        delay(150); // Allow task to exit
    }

    void setMessage(const char* msg) {
        message_.store(msg);
    }

    bool isActive() const {
        return active_.load();
    }

private:
    Display& display_;
    std::atomic<bool> active_{false};
    std::atomic<const char*> message_{"Booting..."};

    static void taskFunc(void* parameter) {
        auto* self = static_cast<BootAnimation*>(parameter);
        int frame = 0;
        while (self->active_.load()) {
            self->display_.showBootStep(self->message_.load(), frame++);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelete(NULL);
    }
};
