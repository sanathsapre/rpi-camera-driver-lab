# Day 7 Learnings — Lab 09: DT Overlay via configfs + pm_runtime stubs
**Date:** April 26, 2026
**Topic:** DT Overlay loading via configfs, gpio-keys binding, pm_runtime stubs in platform driver

---

## 1. DT Overlay loading via configfs

Overlays can be loaded at runtime without rebooting using configfs:

```bash
# Load
mkdir /sys/kernel/config/device-tree/overlays/sanath-gpio-keys
cat sanath-gpio-keys.dtbo > /sys/kernel/config/device-tree/overlays/sanath-gpio-keys/dtbo

# Unload
rmdir /sys/kernel/config/device-tree/overlays/sanath-gpio-keys
```

`mkdir` triggers `of_overlay_fdt_apply()` internally. `rmdir` triggers `of_overlay_remove()`. The kernel handles driver probe and remove automatically — no manual intervention needed.

---

## 2. The overlay that worked

```dts
/dts-v1/;
/plugin/;
/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            gpio_keys_node: gpio-keys {
                compatible = "gpio-keys";
                #address-cells = <1>;
                #size-cells = <0>;
                button@0 {
                    reg = <0>;
                    label = "Sanath Button";
                    gpios = <&gpio 23 1>;
                    linux,code = <28>;
                    debounce-interval = <50>;
                };
            };
        };
    };
};
```

---

## 3. Debug session — wrong graft location

**Symptom:** Overlay status showed `applied`, driver did not probe, no gpio-keys node in `/proc/device-tree`.

**Debug steps:**

```bash
# Step 1 — check overlay status
cat /sys/kernel/config/device-tree/overlays/sanath-gpio-keys/status
# → applied

# Step 2 — check graft path
cat /sys/kernel/config/device-tree/overlays/sanath-gpio-keys/path
# → empty — graft location not resolved

# Step 3 — check if node landed anywhere
find /proc/device-tree -name "gpio-keys"
# → nothing

# Step 4 — check __symbols__ to find where node actually landed
cat /proc/device-tree/__symbols__/gpio_keys_node
# → /soc/gpio@7e200000/gpio-keys
```

**Root cause:** The overlay used `&gpio` as the target. `&gpio` resolves to the BCM2835 GPIO controller node `/soc/gpio@7e200000`. The `gpio-keys` node was grafted inside the GPIO controller — not at the root level. The platform bus scans top-level nodes only, so it never saw the `gpio-keys` device and probe never fired.

**Fix:** Use `target-path = "/"` with `fragment@0` syntax to graft the node at the root level where the platform bus can find it.

**Key lesson:** `&gpio` means "inside the GPIO controller node" — not "using GPIO23". The `gpios = <&gpio 23 1>` property inside the button node is what references GPIO23. These are two completely different things.

---

## 4. gpio-keys as a loadable module

`gpio-keys` was not built into the RPi kernel — it was a `.ko.xz` module. It did not auto-load when the overlay was applied. Had to manually `modprobe gpio_keys` first.

In production this would be handled by udev rules or by building the driver into the kernel image.

---

## 5. Verifying GPIO pin state

```bash
cat /sys/kernel/debug/gpio
```

After overlay load, GPIO23 showed:

```
gpio-23 (GPIO23 | Sanath Button) in lo IRQ ACTIVE LOW
```

This confirmed the driver claimed the pin correctly. The `lo` reading was due to a floating pin with no pull-up — hardware issue, not a driver issue.

---

## 6. pm_runtime stubs in platform driver

Added to `sanath-irq-lab08.c`:

```c
#include <linux/pm_runtime.h>

static int sanath_runtime_suspend(struct device *dev)
{
    dev_info(dev, "runtime suspend\n");
    return 0;
}

static int sanath_runtime_resume(struct device *dev)
{
    dev_info(dev, "runtime resume\n");
    return 0;
}

static const struct dev_pm_ops sanath_pm_ops = {
    .runtime_suspend = sanath_runtime_suspend,
    .runtime_resume  = sanath_runtime_resume,
};
```

In `probe()`:
```c
pm_runtime_set_active(&pdev->dev);
pm_runtime_enable(&pdev->dev);
```

In `remove()`:
```c
pm_runtime_disable(&pdev->dev);
```

---

## 7. pm_runtime behaviour observed

On `insmod` — `runtime suspend` fired immediately after probe. This is correct — usage counter is zero after enable, PM core idles the device instantly. In a real driver you would set an autosuspend delay to prevent this.

On `rmmod` — sequence was:
```
runtime resume   ← pm_runtime_disable() resumes device internally
runtime suspend  ← final suspend before driver unloads
removed          ← remove() runs
```

Clean shutdown sequence — PM core ensures device is in a known state before allowing unload.

---

## Open Questions

- Why does `pm_runtime_disable()` trigger a resume before final suspend during rmmod — what is the exact sequence internally?
- Floating pin reads `lo` by default on BCM2835 — is this a hardware default or kernel GPIO subsystem default?
- `gpio-keys` didn't auto-load when overlay applied — what udev rule would handle this automatically in a production Yocto image?
