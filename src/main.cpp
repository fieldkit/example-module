#include <fk-module.h>

class ExampleModule : public fk::Module {
private:
    fk::TwoWireBus bus{ Wire };

public:
    ExampleModule(fk::ModuleInfo &info);

public:
    fk::ModuleReadingStatus beginReading(fk::PendingSensorReading &pending) override;
};

ExampleModule::ExampleModule(fk::ModuleInfo &info) : Module(bus, info) {
}

fk::ModuleReadingStatus ExampleModule::beginReading(fk::PendingSensorReading &pending) {
    return fk::ModuleReadingStatus();
}

extern "C" {

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(100);
    }

    loginfof("Module", "Starting (%d free)", fk_free_memory());

    fk::ModuleInfo info = {
        fk_module_ModuleType_SENSOR,
        8,
        3,
        "NOAA-CTD",
        { { "Depth", "m" },
          { "Temperature", "°C" },
          { "Conductivity", "µS/cm" }
        },
        { {}, {}, {} },
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
