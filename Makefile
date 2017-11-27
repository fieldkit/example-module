BUILD=build

default: core/config.h all

core/config.h:
	cp core/config.h.template core/config.h

$(BUILD):
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../

all: $(BUILD)
	cd $(BUILD) && make

clean:
	rm -rf $(BUILD)
