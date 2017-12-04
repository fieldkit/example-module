#include <fk-module.h>

class ExampleModule : public fk::Module {
private:

public:
    ExampleModule(fk::ModuleInfo &info);

public:
    void beginReading(fk::SensorReading *readings) override;
    void readingDone(fk::SensorReading *readings) override;
};

ExampleModule::ExampleModule(fk::ModuleInfo &info) : Module(info) {
}

void ExampleModule::beginReading(fk::SensorReading *readings) {
    readingDone(readings);
}

void ExampleModule::readingDone(fk::SensorReading *readings) {
}

extern "C" {

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(100);
    }

    debugfpln("Module", "Starting (%d free)", fk_free_memory());

    fk::ModuleInfo info = {
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
