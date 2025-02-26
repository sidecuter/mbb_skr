# Maibenben Special Keys Reader Kernel Module

![Linux Kernel](https://img.shields.io/badge/Linux_Kernel-4.3+-blue.svg)
![License](https://img.shields.io/badge/License-GPLv2-blue.svg)

Kernel module for reading special function key events on Maibenben laptops. Provides secure access to keyboard status data through sysfs interface.

## Features

- Real-time polling of special key states
- Secure buffer handling with memory sanitization
- Sysfs interface at `/sys/kernel/mbb_skr/data`
- Rate-limited error logging
- Fixed 500ms polling interval

## Compatibility

### Tested Devices
| Device Model       | Status   | Tested Kernel Version |
|--------------------|----------|-----------------------|
| Maibenben P-477    | ✅ Working | 6.12.10-zen    |

### Supported Kernels
**Minimum:** 4.3+  
**Recommended:** 5.4+  

| Distribution      | Support              | Notes                          |
|-------------------|----------------------|--------------------------------|
| Ubuntu 20.04+     | ✅ Full              | LTS versions with 5.4+ kernel  |
| Debian 10+        | ✅ Full              | Requires backports repository  |
| Fedora 32+        | ✅ Full              | Native support for new kernels |
| CentOS/RHEL 8+    | ⚠️ Limited          | Requires ELRepo 5.x kernel     |
| Arch Linux        | ✅ Full              | Rolling release with latest kernel |
| Linux Mint 20+    | ✅ Full              | Based on Ubuntu 20.04          |


## Installation

### Prerequisites
```bash
# Common build tools
#  Debian/Ubuntu
sudo apt install build-essential git
#  Fedora/RHEL
sudo dnf install gcc make git
#  Arch/Manjaro
sudo pacman -S base-devel git

# Kernel headers
#  Debian/Ubuntu
sudo apt install linux-headers-$(uname -r)
#  Fedora/RHEL
sudo dnf install kernel-devel
#  Arch
sudo pacman -S linux-headers
```

### Build Instructions

```bash
git clone https://github.com/yourrepo/mbb_skr.git
cd mbb_skr
make
```

### Loading

```bash
sudo insmod mbb_skr.ko
```

### Verification
```bash
# Check module load
lsmod | grep "mbb_skr"

# Verify sysfs interface
ls -l /sys/kernel/mbb_skr/data

# Expected permissions:
# -r--r--r-- 1 root root 4096 Jul 10 12:34 /sys/kernel/mbb_skr/data

# View key data (32-byte buffer):
xxd /sys/kernel/mbb_skr/data

# Example output:
# 00000000: 010f 0200 0000 0000 0000 0000 0000 0000  ................
# 00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

## Special cases

### Older Kernels (<4.3)

Modify the code:

```diff
- memzero_explicit(evbu_data, EVBU_SIZE);
+ memset(evbu_data, 0, EVBU_SIZE);
+ barrier();
```

## Uninstallation

```bash
sudo rmmod mbb_skr
```

## Troubleshooting

**Module failes to load**:
- Verify kernel headers match running kernel
- Inspect dmesg output
```bash
dmesg | tail -n 20
```
**Permission denied on sysfs**:
```bash
sudo chmod 0444 /sys/kernel/mbb_skr/data
```

**EV20 method execution failed**:
1. Check that you don't have `acpi=off` kernel parameter
2. Check for required ACPI methods:
```bash
# Extract and disassemble ACPI tables
sudo acpidump > acpi.dat
acpixtract -a acpi.dat
iasl -d ssdt1.dat dsdt.dat

# Verify EV20 method exists
grep -E "Method.*EV20" ssdt1.dsl

# Check EVBU buffer declaration
grep "Name.*EVBU" ssdt1.dsl

# Look for EC event methods
grep "_Q[0-9A-F][0-9A-F]" dsdt.dsl
```
3. If above check failed, your model isn't compatible with this module
4. You can try monitor acpi events when pressing special keys and figure by yourself how to adapt this module for your events
```bash
sudo systemctl start acpid
acpi_listen
```

## Contributing

1. Fork the repository
2. Create feature branch (git checkout -b feature/improvement)
3. Commit changes (git commit -am 'Add new feature')
4. Push to branch (git push origin feature/improvement)
5. Create Pull Request

## License

This project is licensed under the GPLv2 License - see the LICENSE file for details.

-------
*Copyright © 2025 Alexander Svobodov. Contains code from Linux kernel ACPI subsystem.*