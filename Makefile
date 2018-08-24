BUILD=build

default: all

$(BUILD): gitdeps
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../

all: $(BUILD)
	cd $(BUILD) && make

gitdeps:
	simple-deps --config src/dependencies.sd

clean:
	rm -rf $(BUILD)

veryclean: clean
	rm -rf gitdeps
