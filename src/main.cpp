#include <fk-module.h>

namespace fk {

Board board{
    {
        0,
        0,
        {
            0,
            0,
            0,
            0,
        },
        {
            0,
            0,
            0,
            0,
        }
    }
};

}

class TakeSensorReadings : public fk::ModuleServicesState {
public:
    const char *name() const override {
        return "TakeSensorReadings";
    }

public:
    void task() override;
};

void TakeSensorReadings::task() {
    for (size_t i = 0; i < 3; ++i) {
        services().readings->done(i, random(10, 20));
    }

    transit<fk::ModuleIdle>();
}

class ExampleModule : public fk::Module<fk::MinimumFlashState> {
private:
    fk::TwoWireBus bus{ Wire };

public:
    ExampleModule(fk::ModuleInfo &info);

public:
    fk::ModuleStates states() override {
        return {
            fk::ModuleFsm::deferred<fk::ConfigureModule>(),
            fk::ModuleFsm::deferred<TakeSensorReadings>()
        };
    }

};

ExampleModule::ExampleModule(fk::ModuleInfo &info) : Module(bus, info) {
}

extern "C" {

void setup() {
    Serial.begin(115200);

    while (!Serial && millis() < 2000) {
        delay(100);
    }

    if (!Serial) {
        // The call to end here seems to free up some memory.
        Serial.end();
        Serial5.begin(115200);
        log_uart_set(Serial5);
    }

    firmware_version_set(FIRMWARE_GIT_HASH);
    firmware_build_set(FIRMWARE_BUILD);
    firmware_compiled_set(DateTime(__DATE__, __TIME__).unixtime());

    loginfof("Module", "Starting (%lu free)", fk_free_memory());

    fk::SensorInfo sensors[3] = {
        { "Depth", "m" },
        { "Temperature", "°C" },
        { "Conductivity", "µS/cm" }
    };

    fk::SensorReading readings[3];

    fk::ModuleInfo info = {
        fk_module_ModuleType_SENSOR,
        8,
        3,
        1,
        "Example Module",
        "fk-ex-module",
        sensors,
        readings,
        0,
        fk_module_RequiredUptime_READINGS_ONLY,
    };

    ExampleModule module(info);

    module.begin();

    while(true) {
        module.tick();
        delay(10);
    }
}

void loop() {
}

}
