CFLAGS = -O2 -Wall
ifeq ($(OS),Windows_NT)
EXEEXT = .exe
endif
TRANSPILED = $(addprefix transpiled/QOA., c cpp cs js py swift ts) transpiled/QOADecoder.java
TEST_DIRS = ../test/bandcamp ../test/oculus_audio_pack ../test/sqam

all: wav2qoa$(EXEEXT) $(TRANSPILED)

wav2qoa$(EXEEXT): wav2qoa.c transpiled/QOA.c
	$(CC) $(CFLAGS) -I transpiled -o $@ wav2qoa.c

$(TRANSPILED): QOA.ci
	mkdir -p $(@D) && cito -o $@ $^

test: test-encoder test-decoder

test-encoder: $(patsubst ../test/%.wav, ../test-ci/%.qoa, $(wildcard $(addsuffix /*.wav, $(TEST_DIRS))))

../test-ci/%.qoa: ../test/%.wav wav2qoa$(EXEEXT)
	mkdir -p $(@D) && ./wav2qoa -o $@ $<

test-decoder: $(patsubst ../test/%.qoa, ../test-ci/%.qoa.wav, $(wildcard $(addsuffix /qoa/*.qoa, $(TEST_DIRS))))

../test-ci/%.qoa.wav: ../test/%.qoa wav2qoa$(EXEEXT)
	mkdir -p $(@D) && ./wav2qoa -o $@ $<

clean:
	$(RM) wav2qoa$(EXEEXT)

.PHONY: test test-encoder test-decoder clean
