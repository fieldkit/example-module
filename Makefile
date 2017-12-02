BUILD=build

default: all

$(BUILD):
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../

all: $(BUILD)
	cd $(BUILD) && make

clean:
	rm -rf $(BUILD)
