#include <fk-module.h>

const uint8_t LED_PIN = 13;

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
    void beginReading() override;
    void readingDone() override;
    void describeSensor(size_t number) override;
};

ExampleModule::ExampleModule() : Module(myInfo) {
}

void ExampleModule::beginReading() {
    readingDone();
}

void ExampleModule::readingDone() {
}

void ExampleModule::describeSensor(size_t number) {
    switch (number) {
    }
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
