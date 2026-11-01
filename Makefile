FAUST ?= faust
FAUSTPROBE ?= faustprobe
PYTHON ?= python3
PRESET ?= nebula
SECONDS ?= 20

.PHONY: test demo params cpp rust clean

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
