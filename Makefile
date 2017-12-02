BUILD=build

default: all

$(BUILD): gitdeps
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../

all: $(BUILD)
	cd $(BUILD) && make

gitdeps:
	simple-deps --config src/arduino-libraries

clean:
	rm -rf $(BUILD)
