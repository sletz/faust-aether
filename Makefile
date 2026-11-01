.DEFAULT_GOAL := help

FAUST ?= faust
FAUSTPROBE ?= faustprobe
PYTHON ?= python3
PRESET ?= nebula
SECONDS ?= 20

.PHONY: help test demo params cpp rust clean

help:
	@printf '%s\n' \
	  'Aether — synthétiseur hybride en Faust' \
	  '' \
	  'Commandes :' \
	  '  make help       Afficher cette aide (également la cible par défaut)' \
	  '  make demo       Générer une démo WAV et sa copie PCM16 pour écoute' \
	  '  make params     Afficher les paramètres des voix' \
	  '  make test       Tester le DSP avec faustprobe et exporter C++/Rust' \
	  '  make web        Compiler les modules WebAssembly avec faust-rs' \
	  '  make web-check  Comparer le WASM à faustprobe et tester les presets web' \
	  '  make web-serve  Servir l’interface sur http://127.0.0.1:$(PORT)/web/' \
	  '  make cpp        Exporter les classes DSP C++ dans generated/' \
	  '  make rust       Exporter les classes DSP Rust dans generated/' \
	  '  make clean      Supprimer generated/ et test-results/' \
	  '' \
	  'Options :' \
	  '  PRESET=$(PRESET)  SECONDS=$(SECONDS)  PORT=$(PORT)  WEB_PRECISION=$(WEB_PRECISION)' \
	  '  FAUST=$(FAUST)  FAUSTPROBE=$(FAUSTPROBE)  FAUSTRS=$(FAUSTRS)  PYTHON=$(PYTHON)' \
	  '' \
	  'Exemples :' \
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
