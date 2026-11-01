.DEFAULT_GOAL := help

FAUST ?= faust
FAUSTPROBE ?= faustprobe
PYTHON ?= python3
PRESET ?= nebula
SECONDS ?= 20

.PHONY: help test demo params cpp rust clean

help:
	@printf '%s\n' \
	  'Aether — hybrid synthesizer in Faust' \
	  '' \
	  'Commands:' \
	  '  make help       Show this help (also the default target)' \
	  '  make demo       Render a WAV demo and its PCM16 listening copy' \
	  '  make params     List voice parameters' \
	  '  make test       Test the DSP with faustprobe and export C++/Rust' \
	  '  make web        Compile WebAssembly modules with faust-rs' \
	  '  make web-check  Compare WASM against faustprobe and test web presets' \
	  '  make web-serve  Serve the interface at http://127.0.0.1:$(PORT)/web/' \
	  '  make cpp        Export C++ DSP classes to generated/' \
	  '  make rust       Export Rust DSP classes to generated/' \
	  '  make clean      Remove generated/ and test-results/' \
	  '' \
	  'Options:' \
	  '  PRESET=$(PRESET)  SECONDS=$(SECONDS)  PORT=$(PORT)  WEB_PRECISION=$(WEB_PRECISION)' \
	  '  FAUST=$(FAUST)  FAUSTPROBE=$(FAUSTPROBE)  FAUSTRS=$(FAUSTRS)  PYTHON=$(PYTHON)' \
	  '' \
	  'Examples:' \
	  '  make demo PRESET=glass SECONDS=12' \
	  '  make web-serve PORT=8090' \
	  '  make -B web WEB_PRECISION=-double'

test:
	FAUST="$(FAUST)" FAUSTPROBE="$(FAUSTPROBE)" $(PYTHON) tests/test_aether.py

demo:
	FAUSTPROBE="$(FAUSTPROBE)" $(PYTHON) scripts/render.py --preset "$(PRESET)" --seconds "$(SECONDS)"

params:
	"$(FAUSTPROBE)" --double -I . --nvoices 12 --list-params aether.dsp

cpp rust:
	mkdir -p generated
	"$(FAUST)" -double -I . -lang $@ aether.dsp -o generated/voice.$(if $(filter rust,$@),rs,cpp)
	"$(FAUST)" -double -I . -lang $@ effects.dsp -o generated/effects.$(if $(filter rust,$@),rs,cpp)

clean:
	rm -rf generated test-results

FAUSTRS ?= faust-rs
PORT ?= 8088
WEB_PRECISION ?= -single
.PHONY: web web-check web-serve
web: web/dsp/voice.wasm web/dsp/effects.wasm

web/dsp/voice.wasm: aether.dsp
	mkdir -p web/dsp
	"$(FAUSTRS)" -lang wasm $(WEB_PRECISION) --timeout 0 -I . aether.dsp -o $@

web/dsp/effects.wasm: aether.dsp effects.dsp
	mkdir -p web/dsp
	"$(FAUSTRS)" -lang wasm $(WEB_PRECISION) --timeout 0 -I . effects.dsp -o $@

web-check: web
	FAUSTPROBE="$(FAUSTPROBE)" node tests/web-check.mjs

web-serve: web
	$(PYTHON) scripts/web_serve.py $(PORT)
