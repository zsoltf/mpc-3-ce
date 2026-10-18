package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/xml"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestCatalogPreservesForeignSettings(t *testing.T) {
	foreign := `<PLUGIN name="Other &amp; Synth" uid="4d705053" manufacturer="Another" file="/foreign.so"/>`
	old := `<PLUGIN name="Old" uid="64436e53" manufacturer="DISTRHO" file="/tmp/old.so"/>`
	before := `<?xml version="1.0"?><PROPERTIES><VALUE name="outside" val="A &amp; B"/><VALUE name="pluginList-arm"><KNOWNPLUGINS>` + foreign + old + old + `</KNOWNPLUGINS></VALUE><!-- retain --></PROPERTIES>`
	out, e := registerCatalog([]byte(before))
	if e != nil {
		t.Fatal(e)
	}
	if !bytes.Contains(out, []byte(foreign)) || !bytes.Contains(out, []byte(`<VALUE name="outside" val="A &amp; B"/>`)) || !bytes.Contains(out, []byte(`<!-- retain -->`)) {
		t.Fatal("unrelated settings changed")
	}
	if bytes.Contains(out, []byte("/tmp/old.so")) {
		t.Fatal("stale owned records retained")
	}
	assertCatalog(t, out, 5)
	again, e := registerCatalog(out)
	if e != nil || !bytes.Equal(out, again) {
		t.Fatal("registration is not byte-idempotent", e)
	}
}

func assertCatalog(t *testing.T, b []byte, expected int) {
	t.Helper()
	d := xml.NewDecoder(bytes.NewReader(b))
	count := 0
	for {
		token, e := d.Token()
		if e == io.EOF {
			break
		}
		if e != nil {
			t.Fatal(e)
		}
		if tag, ok := token.(xml.StartElement); ok && tag.Name.Local == "PLUGIN" {
			count++
		}
	}
	if count != expected {
		t.Fatalf("got %d plugins, expected %d", count, expected)
	}
}

func TestCatalogShapes(t *testing.T) {
	for _, input := range []string{
		`<PROPERTIES></PROPERTIES>`,
		`<PROPERTIES><VALUE name="pluginList-arm"/></PROPERTIES>`,
		`<PROPERTIES><VALUE name="pluginList-arm"><KNOWNPLUGINS/></VALUE></PROPERTIES>`,
	} {
		out, e := registerCatalog([]byte(input))
		if e != nil {
			t.Fatal(input, e)
		}
		assertCatalog(t, out, 4)
	}
	for _, input := range []string{
		`<PROPERTIES>`,
		`<WRONG/>`,
		`<PROPERTIES><VALUE name="pluginList-arm" val="serialized scalar"/></PROPERTIES>`,
		`<PROPERTIES><VALUE name="pluginList-arm"/><VALUE name="pluginList-arm"/></PROPERTIES>`,
		`<PROPERTIES><VALUE name="pluginList-arm"><KNOWNPLUGINS/><KNOWNPLUGINS/></VALUE></PROPERTIES>`,
	} {
		if _, e := registerCatalog([]byte(input)); e == nil {
			t.Fatal("accepted invalid or ambiguous input", input)
		}
	}
	if _, e := registerCatalog(bytes.Repeat([]byte{' '}, maxSettings+1)); e == nil {
		t.Fatal("accepted oversized settings")
	}
}

// Optional private native fixture. The test never prints settings content;
// its result can be decoded independently before native admission.
func TestNativeCatalog(t *testing.T) {
	path := os.Getenv("PLUGIN_SETUP_NATIVE_SETTINGS")
	if path == "" {
		t.Skip("no private native fixture supplied")
	}
	b, e := os.ReadFile(path)
	if e != nil {
		t.Fatal(e)
	}
	out, e := registerCatalog(b)
	if e != nil {
		t.Fatal(e)
	}
	if e := os.WriteFile(path+".registered", out, 0600); e != nil {
		t.Fatal(e)
	}
	again, e := registerCatalog(out)
	if e != nil || !bytes.Equal(out, again) {
		t.Fatal("native fixture is not idempotent", e)
	}
}

func TestCompleteBundleAdmission(t *testing.T) {
	path := filepath.Join(t.TempDir(), "bundle")
	b := []byte("complete expected bundle")
	digest := sha256.Sum256(b)
	spec := bundleSpec{1, int64(len(b)), hex.EncodeToString(digest[:])}
	if e := os.WriteFile(path, b, 0600); e != nil {
		t.Fatal(e)
	}
	if _, e := checkBundle(path, spec); e != nil {
		t.Fatal(e)
	}
	for _, other := range [][]byte{b[:len(b)-1], []byte(strings.Repeat("x", len(b)))} {
		if e := os.WriteFile(path, other, 0600); e != nil {
			t.Fatal(e)
		}
		if _, e := checkBundle(path, spec); e == nil {
			t.Fatal("accepted truncated or changed bundle")
		}
	}
}

func TestCardMountEscapes(t *testing.T) {
	got := parseMounts([]byte(`63 20 8:1 / /media/MPC\040Card rw,nosuid,nodev,noexec - exfat /dev/sda1 rw`))
	if len(got) != 1 || got[0].Path != "/media/MPC Card" || got[0].Device != "8:1" || got[0].Type != "exfat" || got[0].Source != "/dev/sda1" {
		t.Fatal(got)
	}
}
