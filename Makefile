BUILD=build

default: src/config.h all

$(BUILD):
	mkdir -p $(BUILD)

src/config.h:
	cp src/config.h.template src/config.h

all: $(BUILD)
	cd $(BUILD) && cmake ../
	cd $(BUILD) && make

clean:
	rm -rf $(BUILD)
