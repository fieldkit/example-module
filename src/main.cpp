#include <fk-module.h>

fk::SensorInfo mySensors[] = {
    {
        .sensor = 0,
        .name = "Depth",
        .unitOfMeasure = "m",
    },
    {
        .sensor = 1,
        .name = "Temperature",
        .unitOfMeasure = "°C",
    },
    {
        .sensor = 2,
        .name = "Conductivity",
        .unitOfMeasure = "µS/cm",
    }
};

fk::ModuleInfo myInfo = {
    .address = 8,
    .numberOfSensors = 3,
    .name = "NOAA-CTD",
    .sensors = mySensors,
};

class ExampleModule : public fk::Module {
private:

public:
    ExampleModule();

public:
    void beginReading(fk::SensorReading *readings) override;
    void readingDone(fk::SensorReading *readings) override;
};

ExampleModule::ExampleModule() : Module(myInfo) {
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

    ExampleModule module;

    module.begin();

    while(true) {
        module.tick();
        delay(10);
    }
}

void loop() {
}

}
