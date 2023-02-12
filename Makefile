TRANSPILED = $(addprefix transpiled/QOA., c cpp cs js py swift ts) transpiled/QOADecoder.java

all: $(TRANSPILED)

$(TRANSPILED): QOA.ci
	mkdir -p $(@D) && cito -o $@ $^