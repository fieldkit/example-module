#include <fk-module.h>

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

    while (!Serial) {
        delay(100);
    }

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
        readings
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
