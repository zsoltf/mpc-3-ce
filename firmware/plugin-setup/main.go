// A finite admission helper for the existing MPC session owner. It owns no
// daemon, audio device, media automounter, or recurring watcher.
package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

const statePath = "/run/mpclearn/plugin-mount.json"
const settingsPath = "/data/Settings/MPC/MPC.settings"
const settingsNext = "/data/mpclearn/plugin-settings.next"
const cardRelative = "MPCLEARN/plugins.squashfs"

var admissionContext = context.Background()

type bundleSpec struct {
	Version int    `json:"version"`
	Bytes   int64  `json:"bytes"`
	SHA256  string `json:"sha256"`
}
type fileIdentity struct {
	Device, Inode   uint64
	Bytes, Modified int64
}
type mountIdentity struct{ ID, Device, Source, Path, Type, Options string }
type receipt struct {
	Version int
	SHA256  string
	Card    mountIdentity
	Bundle  string
	File    fileIdentity
	View    mountIdentity
}

func boundedRead(path string, limit int64) ([]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	b, err := io.ReadAll(io.LimitReader(f, limit+1))
	if err != nil {
		return nil, err
	}
	if int64(len(b)) > limit {
		return nil, fmt.Errorf("%s exceeds its size bound", path)
	}
	return b, nil
}

func regular(path string) (fileIdentity, os.FileInfo, error) {
	i, err := os.Lstat(path)
	if err != nil {
		return fileIdentity{}, nil, err
	}
	if !i.Mode().IsRegular() {
		return fileIdentity{}, nil, fmt.Errorf("not a regular file: %s", path)
	}
	s := i.Sys().(*syscall.Stat_t)
	return fileIdentity{uint64(s.Dev), uint64(s.Ino), i.Size(), i.ModTime().UnixNano()}, i, nil
}

func ownDirectory(path string) error {
	err := os.Mkdir(path, 0700)
	if err != nil && !errors.Is(err, os.ErrExist) {
		return err
	}
	i, err := os.Lstat(path)
	if err != nil {
		return err
	}
	if !i.IsDir() || i.Mode().Perm() != 0700 || i.Sys().(*syscall.Stat_t).Uid != 0 {
		return fmt.Errorf("not an owned directory: %s", path)
	}
	return nil
}

func command(timeout time.Duration, name string, args ...string) ([]byte, error) {
	ctx, cancel := context.WithTimeout(admissionContext, timeout)
	defer cancel()
	cmd := exec.CommandContext(ctx, name, args...)
	cmd.WaitDelay = time.Second
	var out limitedBuffer
	cmd.Stdout = &out
	cmd.Stderr = &out
	err := cmd.Run()
	return out.Bytes(), err
}

type limitedBuffer struct{ bytes.Buffer }

func (b *limitedBuffer) Write(p []byte) (int, error) {
	n := len(p)
	if b.Len() < 4096 {
		keep := 4096 - b.Len()
		if len(p) > keep {
			p = p[:keep]
		}
		_, _ = b.Buffer.Write(p)
	}
	return n, nil
}

func unescapeMount(s string) string {
	return strings.NewReplacer(`\040`, " ", `\011`, "\t", `\012`, "\n", `\134`, `\`).Replace(s)
}
func parseMounts(data []byte) []mountIdentity {
	var mounts []mountIdentity
	for _, line := range strings.Split(string(data), "\n") {
		parts := strings.SplitN(line, " - ", 2)
		if len(parts) != 2 {
			continue
		}
		left, right := strings.Fields(parts[0]), strings.Fields(parts[1])
		if len(left) < 6 || len(right) < 3 {
			continue
		}
		mounts = append(mounts, mountIdentity{left[0], left[2], unescapeMount(right[1]), unescapeMount(left[4]), right[0], left[5]})
	}
	return mounts
}
func mounts() ([]mountIdentity, error) {
	b, e := boundedRead("/proc/self/mountinfo", 1<<20)
	if e != nil {
		return nil, e
	}
	return parseMounts(b), nil
}
func mounted(path string) (mountIdentity, error) {
	ms, e := mounts()
	if e != nil {
		return mountIdentity{}, e
	}
	for _, m := range ms {
		if m.Path == path {
			return m, nil
		}
	}
	return mountIdentity{}, os.ErrNotExist
}
func option(options, wanted string) bool {
	for _, v := range strings.Split(options, ",") {
		if v == wanted {
			return true
		}
	}
	return false
}

func mpcStopped() error {
	paths, e := filepath.Glob("/proc/[0-9]*/exe")
	if e != nil {
		return e
	}
	for _, p := range paths {
		target, _ := os.Readlink(p)
		if target == "/usr/bin/MPC" || target == "/usr/bin/MPC (deleted)" {
			return errors.New("MPC must be stopped before plugin admission")
		}
	}
	return nil
}

func specification() (bundleSpec, error) {
	exe, e := os.Executable()
	if e != nil {
		return bundleSpec{}, e
	}
	b, e := boundedRead(filepath.Join(filepath.Dir(exe), "plugin-bundle.json"), 4096)
	if e != nil {
		return bundleSpec{}, e
	}
	var s bundleSpec
	e = json.Unmarshal(b, &s)
	if e != nil {
		return s, e
	}
	d, e := hex.DecodeString(s.SHA256)
	if e != nil || len(d) != 32 || s.Version != 1 || s.Bytes <= 0 || s.Bytes > 512<<20 {
		return s, errors.New("invalid image bundle specification")
	}
	return s, nil
}

// Stock edisksd remains the sole automounter. The card's name and /dev/sdX
// allocation are observations, not saved configuration or selection keys.
func discoverCard() (mountIdentity, string, error) {
	if _, e := command(8*time.Second, "systemctl", "start", "edisksd.service"); e != nil {
		return mountIdentity{}, "", fmt.Errorf("disk service: %w", e)
	}
	_, _ = command(5*time.Second, "udevadm", "settle", "--timeout=4")
	entries, e := os.ReadDir("/sys/class/block")
	if e != nil {
		return mountIdentity{}, "", e
	}
	var found []mountIdentity
	for _, entry := range entries {
		if e := admissionContext.Err(); e != nil {
			return mountIdentity{}, "", e
		}
		base := filepath.Join("/sys/class/block", entry.Name())
		if _, e := os.Stat(filepath.Join(base, "partition")); e != nil {
			continue
		}
		dev := "/dev/" + entry.Name()
		props, e := command(2*time.Second, "udevadm", "info", "--query=property", "--name="+dev)
		if e != nil || !bytes.Contains(append([]byte("\n"), props...), []byte("\nID_DRIVE_FLASH_SD=1\n")) {
			continue
		}
		resolved, e := filepath.EvalSymlinks(base)
		if e != nil {
			continue
		}
		removable, e := boundedRead(filepath.Join(filepath.Dir(resolved), "removable"), 32)
		if e != nil || strings.TrimSpace(string(removable)) != "1" {
			continue
		}
		// AlreadyMounted and InProgress are settled by the mount readback below.
		_, _ = command(8*time.Second, "edisksctl", "mount", dev)
		deviceBytes, e := boundedRead(filepath.Join(base, "dev"), 32)
		if e != nil {
			continue
		}
		device := strings.TrimSpace(string(deviceBytes))
		var card mountIdentity
		for attempt := 0; attempt < 20; attempt++ {
			ms, e := mounts()
			if e != nil {
				return card, "", e
			}
			for _, m := range ms {
				if m.Device == device && strings.HasPrefix(m.Path, "/media/") && (m.Type == "exfat" || m.Type == "vfat" || m.Type == "ext4") {
					card = m
					break
				}
			}
			if card.Path != "" {
				break
			}
			time.Sleep(100 * time.Millisecond)
		}
		if card.Path == "" {
			continue
		}
		path := filepath.Join(card.Path, cardRelative)
		if _, _, e := regular(path); e == nil {
			found = append(found, card)
		}
	}
	if len(found) != 1 {
		return mountIdentity{}, "", fmt.Errorf("expected one SD-card bundle, found %d", len(found))
	}
	return found[0], filepath.Join(found[0].Path, cardRelative), nil
}

func checkBundle(path string, s bundleSpec) (fileIdentity, error) {
	before, _, e := regular(path)
	if e != nil {
		return before, e
	}
	if before.Bytes != s.Bytes {
		return before, errors.New("incomplete or mismatched plugin bundle size")
	}
	f, e := os.Open(path)
	if e != nil {
		return before, e
	}
	defer f.Close()
	h := sha256.New()
	n, e := io.Copy(h, io.LimitReader(f, s.Bytes+1))
	if e != nil {
		return before, e
	}
	if n != s.Bytes || hex.EncodeToString(h.Sum(nil)) != s.SHA256 {
		return before, errors.New("plugin bundle hash mismatch")
	}
	after, _, e := regular(path)
	if e != nil {
		return before, e
	}
	if before != after {
		return before, errors.New("bundle changed during admission")
	}
	return before, nil
}

func readReceipt(s bundleSpec) (receipt, error) {
	var r receipt
	b, e := boundedRead(statePath, 4096)
	if e != nil {
		return r, e
	}
	e = json.Unmarshal(b, &r)
	if e != nil {
		return r, e
	}
	if r.Version != 1 || r.SHA256 != s.SHA256 || r.Bundle != filepath.Join(r.Card.Path, cardRelative) {
		return r, errors.New("admission receipt belongs to another bundle")
	}
	card, e := mounted(r.Card.Path)
	if e != nil || card != r.Card {
		return r, errors.New("admitted SD mount changed")
	}
	identity, _, e := regular(r.Bundle)
	if e != nil || identity != r.File {
		return r, errors.New("admitted SD bundle changed")
	}
	view, e := mounted(pluginRoot)
	if e != nil || view != r.View || view.Type != "squashfs" || !option(view.Options, "ro") {
		return r, errors.New("plugin mount changed")
	}
	for _, state := range engineStates {
		if e := state.ready(); e != nil {
			return r, e
		}
	}
	return r, nil
}

type engineStateSpec struct {
	path  string
	bytes uint64
	links map[string]string
}

var engineStates = []engineStateSpec{
	{"/tmp/mpclearn-cardinal-touch", 128 << 20, map[string]string{"library": pluginRoot + "/Cardinal.vst/resources/patches"}},
	{"/tmp/mpclearn-pd-touch", 64 << 20, map[string]string{"library": pluginRoot + "/pd/library", "assets": pluginRoot + "/pd/assets"}},
}

func (s engineStateSpec) ready() error {
	m, e := mounted(s.path)
	if e != nil || m.Type != "tmpfs" {
		return fmt.Errorf("engine state is not tmpfs: %s", s.path)
	}
	var st syscall.Statfs_t
	if e := syscall.Statfs(s.path, &st); e != nil {
		return e
	}
	if uint64(st.Blocks)*uint64(st.Bsize) != s.bytes {
		return fmt.Errorf("engine state size cap differs: %s", s.path)
	}
	for name, target := range s.links {
		if link, e := os.Readlink(filepath.Join(s.path, name)); e != nil || link != target {
			return fmt.Errorf("engine library link differs: %s", name)
		}
	}
	return nil
}

func linkLibrary(path, target string) error {
	if link, e := os.Readlink(path); e == nil {
		if link == target {
			return nil
		}
		return fmt.Errorf("library link differs: %s", path)
	}
	return os.Symlink(target, path)
}
func (s engineStateSpec) prepare() error {
	path := s.path
	if e := ownDirectory(path); e != nil {
		return e
	}
	m, e := mounted(path)
	if errors.Is(e, os.ErrNotExist) {
		if out, e := command(3*time.Second, "mount", "-t", "tmpfs", "-o", fmt.Sprintf("size=%d,mode=0700,nosuid,nodev", s.bytes), "tmpfs", path); e != nil {
			return fmt.Errorf("state mount: %s: %w", out, e)
		}
	} else if e != nil {
		return e
	} else if m.Type != "tmpfs" {
		return errors.New("engine state is not tmpfs")
	}
	for name, target := range s.links {
		if e := linkLibrary(filepath.Join(path, name), target); e != nil {
			return e
		}
	}
	return s.ready()
}

func registerSettings() error {
	before, info, e := regular(settingsPath)
	if e != nil {
		return e
	}
	if before.Bytes > maxSettings {
		return errors.New("settings too large")
	}
	b, e := boundedRead(settingsPath, maxSettings)
	if e != nil {
		return e
	}
	updated, e := registerCatalog(b)
	if e != nil {
		return e
	}
	if bytes.Equal(b, updated) {
		return nil
	}
	if e = ownDirectory("/data/mpclearn"); e != nil {
		return e
	}
	// The /data path names the same inode as MPC's /media/az01-internal
	// alias. Keeping both rename paths on this mount makes replacement atomic.
	if _, i, previous := regular(settingsNext); previous == nil {
		if i.Size() > maxSettings || i.Sys().(*syscall.Stat_t).Uid != 0 {
			return errors.New("unexpected plugin settings staging file")
		}
		if e = os.Remove(settingsNext); e != nil {
			return e
		}
	} else if !errors.Is(previous, os.ErrNotExist) {
		return previous
	}
	f, e := os.OpenFile(settingsNext, os.O_WRONLY|os.O_CREATE|os.O_EXCL, info.Mode().Perm())
	if e != nil {
		return e
	}
	defer os.Remove(settingsNext)
	st := info.Sys().(*syscall.Stat_t)
	if e = f.Chmod(info.Mode().Perm()); e == nil {
		e = f.Chown(int(st.Uid), int(st.Gid))
	}
	if e == nil {
		_, e = f.Write(updated)
	}
	if e == nil {
		e = f.Sync()
	}
	closeErr := f.Close()
	if e == nil {
		e = closeErr
	}
	if e != nil {
		return e
	}
	after, _, e := regular(settingsPath)
	if e != nil {
		return e
	}
	if before != after {
		return errors.New("settings changed outside stopped admission")
	}
	if e = os.Rename(settingsNext, settingsPath); e != nil {
		return e
	}
	d, e := os.Open(filepath.Dir(settingsPath))
	if e != nil {
		return e
	}
	defer d.Close()
	return d.Sync()
}

func prepare(s bundleSpec) error {
	if e := mpcStopped(); e != nil {
		return e
	}
	if _, e := readReceipt(s); e == nil {
		return registerSettings()
	}
	if _, e := os.Lstat(statePath); e == nil {
		return errors.New("admitted media changed; reboot before admitting another card")
	}
	card, bundle, e := discoverCard()
	if e != nil {
		return e
	}
	identity, e := checkBundle(bundle, s)
	if e != nil {
		return e
	}
	again, e := mounted(card.Path)
	if e != nil || again != card {
		return errors.New("SD mount changed during admission")
	}
	if e = ownDirectory(pluginRoot); e != nil {
		return e
	}
	if _, e = mounted(pluginRoot); !errors.Is(e, os.ErrNotExist) {
		return errors.New("plugin mount already exists without its receipt")
	}
	if out, e := command(5*time.Second, "mount", "-t", "squashfs", "-o", "loop,ro,nosuid,nodev", bundle, pluginRoot); e != nil {
		return fmt.Errorf("plugin mount: %s: %w", out, e)
	}
	view, e := mounted(pluginRoot)
	if e != nil {
		return e
	}
	if view.Type != "squashfs" || !option(view.Options, "ro") {
		return errors.New("plugin mount is not read-only SquashFS")
	}
	for _, state := range engineStates {
		if e = state.prepare(); e != nil {
			return e
		}
	}
	r := receipt{1, s.SHA256, card, bundle, identity, view}
	data, e := json.Marshal(r)
	if e != nil {
		return e
	}
	if e = os.WriteFile(statePath+".next", append(data, '\n'), 0600); e != nil {
		return e
	}
	if e = os.Rename(statePath+".next", statePath); e != nil {
		return e
	}
	if _, e = readReceipt(s); e != nil {
		return e
	}
	if e = registerSettings(); e != nil {
		return e
	}
	fmt.Println("Pd/Cardinal SD bundle admitted; engines remain MPC-owned")
	return nil
}

func main() {
	if os.Geteuid() != 0 || len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "plugin-setup prepare|ready (root)")
		os.Exit(2)
	}
	// Bound the entire optional admission, including card reads. The context
	// first cancels child commands; the final deadline lets base MPC start even
	// if a removed card leaves a filesystem read blocked.
	limit := 35 * time.Second
	if os.Args[1] == "ready" {
		limit = 3 * time.Second
	}
	var cancel context.CancelFunc
	admissionContext, cancel = context.WithTimeout(context.Background(), limit)
	defer cancel()
	deadline := time.AfterFunc(limit+2*time.Second, func() {
		fmt.Fprintln(os.Stderr, "Pd/Cardinal unavailable: admission timed out")
		os.Exit(3)
	})
	defer deadline.Stop()
	s, e := specification()
	if e == nil {
		switch os.Args[1] {
		case "prepare":
			e = prepare(s)
		case "ready":
			_, e = readReceipt(s)
		default:
			e = errors.New("unknown plugin setup operation")
		}
	}
	if e != nil {
		fmt.Fprintln(os.Stderr, "Pd/Cardinal unavailable:", e)
		os.Exit(3)
	}
}
