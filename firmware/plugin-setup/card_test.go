package main

import "testing"

// Sysfs paths and udev properties as on Gen1 hardware: the Live II's card
// reader (Akai's 65-smex-external-sd.rules), the MPC One's slot on the SoC SD
// controller, that controller as internal storage (90-edisks-az01.rules) and
// the internal eMMC and SATA disk.
func TestEligibleSD(t *testing.T) {
	const reader = "/sys/devices/platform/ff500000.usb/usb1/1-1/1-1.1/1-1.1:1.0/host0/target0:0:0/0:0:0:0/block/sda/sda1"
	const slot = "/sys/devices/platform/ff0c0000.dwmmc/mmc_host/mmc1/mmc1:aaaa/block/mmcblk1/mmcblk1p1"
	const emmc = "/sys/devices/platform/ff0f0000.dwmmc/mmc_host/mmc0/mmc0:0001/block/mmcblk0/mmcblk0p7"
	const sata = "/sys/devices/platform/ff500000.usb/usb1/1-1/1-1.4/1-1.4:1.0/host1/target1:0:0/1:0:0:0/block/sdb/sdb1"
	for _, c := range []struct {
		name, path, props, removable string
		want                         bool
	}{
		{"Live II card reader", reader, "DEVTYPE=partition\nID_DRIVE_FLASH_SD=1\nID_FS_TYPE=exfat\n", "1", true},
		{"reader property on a fixed disk", reader, "ID_DRIVE_FLASH_SD=1\n", "0", false},
		{"removable disk that is not an SD reader", sata, "ID_DRIVE_FLASH_SDHC=1\nID_FS_TYPE=exfat\n", "1", false},
		{"MPC One slot", slot, "DEVTYPE=partition\nID_PATH=platform-ff0c0000.dwmmc\nID_FS_TYPE=exfat\n", "0", true},
		{"MPC One slot, last property", slot, "ID_PATH=platform-ff0c0000.dwmmc", "", true},
		{"slot as internal storage", slot, "ID_PATH=platform-ff0c0000.dwmmc\nEDISKSD_REMOVABLE=0\nEDISKSD_IGNORE=1\n", "0", false},
		{"slot ignored, last property", slot, "ID_PATH=platform-ff0c0000.dwmmc\nEDISKSD_IGNORE=1", "0", false},
		{"slot ignore value is not 1", slot, "EDISKSD_IGNORE=10\n", "0", true},
		{"internal eMMC", emmc, "ID_PATH=platform-ff0f0000.dwmmc\nEDISKSD_IGNORE=1\n", "0", false},
		{"internal eMMC without udev ignore", emmc, "ID_PATH=platform-ff0f0000.dwmmc\n", "1", false},
	} {
		if got := eligibleSD(c.path, []byte(c.props), c.removable); got != c.want {
			t.Errorf("%s: eligibleSD = %v, want %v", c.name, got, c.want)
		}
	}
}
