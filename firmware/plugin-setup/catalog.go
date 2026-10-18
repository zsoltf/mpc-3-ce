package main

import (
	"bytes"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"sort"
	"strings"
)

const pluginRoot = "/run/mpclearn/plugins"
const maxSettings = 16 << 20

type plugin struct{ file, name, maker, uid, category, instrument, inputs string }

var plugins = []plugin{
	{"pd/plugins/pd-sketch-synth.so", "Pd Sketch Synth", "mpclearn / Pure Data", "4d705053", "Synth", "1", "0"},
	{"pd/plugins/pd-sketch-fx.so", "Pd Sketch FX", "mpclearn / Pure Data", "4d705046", "Effect", "0", "2"},
	{"Cardinal.vst/CardinalSynth.so", "Cardinal Synth", "DISTRHO", "64436e53", "Synth", "1", "0"},
	{"Cardinal.vst/CardinalFX.so", "Cardinal FX", "DISTRHO", "64436e46", "Effect", "0", "2"},
}

func attribute(e xml.StartElement, name string) string {
	for _, a := range e.Attr {
		if a.Name.Local == name {
			return a.Value
		}
	}
	return ""
}

func ownedPlugin(e xml.StartElement) bool {
	for _, p := range plugins {
		if attribute(e, "uid") == p.uid && attribute(e, "manufacturer") == p.maker {
			return true
		}
	}
	return false
}

func currentPlugin(e xml.StartElement) bool {
	for _, p := range plugins {
		if attribute(e, "uid") == p.uid && attribute(e, "manufacturer") == p.maker &&
			attribute(e, "file") == pluginRoot+"/"+p.file && attribute(e, "name") == p.name &&
			attribute(e, "format") == "VST" && attribute(e, "isInstrument") == p.instrument &&
			attribute(e, "numInputs") == p.inputs && attribute(e, "numOutputs") == "2" {
			return true
		}
	}
	return false
}

func descriptions() []byte {
	var b bytes.Buffer
	for _, p := range plugins {
		fmt.Fprintf(&b, `\n      <PLUGIN name="%s" format="VST" category="%s" manufacturer="%s" version="1" file="%s/%s" uid="%s" isInstrument="%s" fileTime="0" infoUpdateTime="0" numInputs="%s" numOutputs="2" isShell="0"/>`, p.name, p.category, p.maker, pluginRoot, p.file, p.uid, p.instrument, p.inputs)
	}
	return bytes.ReplaceAll(b.Bytes(), []byte(`\n`), []byte("\n"))
}

// Only our four PLUGIN records change. Foreign entries and every other byte
// outside that catalog retain their original spelling and serialization.
func registerCatalog(input []byte) ([]byte, error) {
	if len(input) > maxSettings {
		return nil, errors.New("settings exceed the size bound")
	}
	type edit struct {
		start, end int
		data       []byte
	}
	var edits []edit
	d := xml.NewDecoder(bytes.NewReader(input))
	depth, valueDepth, catalogDepth, ownedDepth := 0, -1, -1, -1
	values, catalogs, rootClose, ownedStart, catalogStart, catalogOpenEnd := 0, 0, -1, 0, 0, 0
	rootSeen, valueEmpty := false, false
	valueStart, valueOpenEnd := 0, 0
	owned, current := map[string]int{}, map[string]int{}
	for {
		before := int(d.InputOffset())
		t, err := d.Token()
		if errors.Is(err, io.EOF) {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("invalid settings XML: %w", err)
		}
		after := int(d.InputOffset())
		switch e := t.(type) {
		case xml.StartElement:
			if depth == 0 {
				if rootSeen || e.Name.Local != "PROPERTIES" {
					return nil, errors.New("expected one PROPERTIES root")
				}
				rootSeen = true
			}
			if depth == 1 && e.Name.Local == "VALUE" && attribute(e, "name") == "pluginList-arm" {
				for _, a := range e.Attr {
					if a.Name.Local == "val" {
						return nil, errors.New("plugin catalog is a scalar, not native XML")
					}
				}
				values++
				valueDepth = depth
				valueStart = before
				valueOpenEnd = after
				valueEmpty = bytes.HasSuffix(bytes.TrimSpace(input[before:after]), []byte("/>"))
			}
			if valueDepth >= 0 && depth == valueDepth+1 && e.Name.Local == "KNOWNPLUGINS" {
				catalogs++
				catalogDepth = depth
				catalogStart = before
				catalogOpenEnd = after
			}
			if catalogDepth >= 0 && depth == catalogDepth+1 && e.Name.Local == "PLUGIN" && ownedPlugin(e) {
				ownedDepth = depth
				ownedStart = before
				owned[attribute(e, "uid")]++
				if currentPlugin(e) {
					current[attribute(e, "uid")]++
				}
			}
			depth++
		case xml.EndElement:
			depth--
			if depth == ownedDepth {
				edits = append(edits, edit{ownedStart, after, nil})
				ownedDepth = -1
			}
			if depth == catalogDepth {
				head := input[catalogStart:catalogOpenEnd]
				if bytes.HasSuffix(bytes.TrimSpace(head), []byte("/>")) {
					replacement := append([]byte(nil), bytes.TrimSuffix(bytes.TrimSpace(head), []byte("/>"))...)
					replacement = append(replacement, '>')
					replacement = append(replacement, descriptions()...)
					replacement = append(replacement, []byte("\n    </KNOWNPLUGINS>")...)
					edits = append(edits, edit{catalogStart, after, replacement})
				} else {
					edits = append(edits, edit{before, before, append(descriptions(), '\n')})
				}
				catalogDepth = -1
			}
			if depth == valueDepth {
				if catalogs == 0 {
					content := append([]byte("<KNOWNPLUGINS>"), descriptions()...)
					content = append(content, []byte("\n    </KNOWNPLUGINS>")...)
					if valueEmpty {
						head := bytes.TrimSuffix(bytes.TrimSpace(input[valueStart:valueOpenEnd]), []byte("/>"))
						body := append(append([]byte(nil), head...), '>')
						body = append(body, content...)
						body = append(body, []byte("</VALUE>")...)
						edits = append(edits, edit{valueStart, after, body})
					} else {
						edits = append(edits, edit{before, before, content})
					}
				}
				valueDepth = -1
			}
			if depth == 0 {
				rootClose = before
			}
		}
	}
	if !rootSeen || depth != 0 || rootClose < 0 || values > 1 || catalogs > 1 {
		return nil, errors.New("ambiguous or incomplete plugin catalog")
	}
	idempotent := values == 1 && catalogs == 1
	for _, p := range plugins {
		if owned[p.uid] != 1 || current[p.uid] != 1 {
			idempotent = false
		}
	}
	if idempotent {
		return append([]byte(nil), input...), nil
	}
	if values == 0 {
		data := append([]byte("  <VALUE name=\"pluginList-arm\"><KNOWNPLUGINS>"), descriptions()...)
		data = append(data, []byte("\n    </KNOWNPLUGINS></VALUE>\n")...)
		edits = append(edits, edit{rootClose, rootClose, data})
	}
	sort.SliceStable(edits, func(i, j int) bool { return edits[i].start > edits[j].start })
	result := append([]byte(nil), input...)
	for _, e := range edits {
		result = append(result[:e.start], append(append([]byte(nil), e.data...), result[e.end:]...)...)
	}
	if len(result) > maxSettings {
		return nil, errors.New("updated settings exceed the size bound")
	}
	// The native catalog codec is XML. Validate the actual result independently
	// of the offset calculations before the stopped session replaces its file.
	decoder := xml.NewDecoder(strings.NewReader(string(result)))
	for {
		_, err := decoder.Token()
		if errors.Is(err, io.EOF) {
			break
		}
		if err != nil {
			return nil, err
		}
	}
	return result, nil
}
