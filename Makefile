BUILD=build

default: core/config.h all

$(BUILD):
	mkdir -p $(BUILD)

core/config.h:
	cp core/config.h.template core/config.h

all: $(BUILD)
	cd $(BUILD) && cmake ../
	cd $(BUILD) && make

clean:
	rm -rf $(BUILD)
