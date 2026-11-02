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
	  '  make juce-export  Export the instrument with faust2juce to generated/juce/aether' \
	  '  make juce       Build the AU/VST3/standalone plugin with the custom JUCE editor' \
	  '  make juce-run   Build and open the standalone application' \
	  '  make juce-snapshot  Play a chord offscreen and render the editor to test-results/juce' \
	  '  make clean      Remove generated/ and test-results/' \
	  '' \
	  'Options:' \
	  '  PRESET=$(PRESET)  SECONDS=$(SECONDS)  PORT=$(PORT)  WEB_PRECISION=$(WEB_PRECISION)' \
	  '  FAUST=$(FAUST)  FAUSTPROBE=$(FAUSTPROBE)  FAUSTRS=$(FAUSTRS)  PYTHON=$(PYTHON)' \
	  '  JUCE_DIR=$(JUCE_DIR)  (empty: CMake downloads JUCE 8.0.10)' \
	  '' \
	  'Examples:' \
	  '  make demo PRESET=glass SECONDS=12' \
	  '  make web-serve PORT=8090' \
	  '  make -B web WEB_PRECISION=-double' \
	  '  make juce JUCE_DIR=~/JUCE'

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

# JUCE export: faust2juce writes <dsp name>/ next to the .dsp and deletes any
# existing entry with that name, so it runs on a copy inside generated/juce.
JUCE_DIR ?=
JUCE_BUILD = generated/juce/build
JUCE_APP = $(JUCE_BUILD)/Aether_artefacts/Release/Standalone/Aether.app
.PHONY: juce-export juce juce-run juce-snapshot
juce-export: generated/juce/aether/FaustPluginProcessor.cpp

generated/juce/aether/FaustPluginProcessor.cpp: aether.dsp juce/patch_processor.py
	mkdir -p generated/juce
	cp aether.dsp generated/juce/aether.dsp
	cd generated/juce && faust2juce -nvoices 12 -effect auto -midi aether.dsp
	$(PYTHON) juce/patch_processor.py $@

juce: juce-export
	cmake -S juce -B $(JUCE_BUILD) -DCMAKE_BUILD_TYPE=Release $(if $(JUCE_DIR),-DJUCE_DIR="$(JUCE_DIR)")
	cmake --build $(JUCE_BUILD) --config Release --parallel

juce-run: juce
	open "$(JUCE_APP)"

juce-snapshot: juce
	cmake --build $(JUCE_BUILD) --config Release --parallel --target AetherSnapshot
	$(JUCE_BUILD)/AetherSnapshot_artefacts/Release/AetherSnapshot test-results/juce
