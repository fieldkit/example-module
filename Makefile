BUILD=build

default: all

$(BUILD): gitdeps
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../

all: $(BUILD)
	$(MAKE) -C $(BUILD)

gitdeps:
	simple-deps --config src/dependencies.sd

clean:
	rm -rf $(BUILD)

veryclean: clean
	rm -rf gitdeps
