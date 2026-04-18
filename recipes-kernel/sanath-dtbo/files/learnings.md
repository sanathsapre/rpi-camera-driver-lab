# Day 3 Learnings — Lab 09: IRQ Driver on RPi3
**Date:** April 20, 2026  
**Topic:** Platform Driver + GPIO Interrupt on RPi3

---

## 1. Platform Driver Model

A platform driver is used to manage components connected directly to the SoC, rather than through a specialised bus like I2C or SPI. Such devices are said to be connected to the **virtual platform bus**.

The kernel matches a platform driver to a device using the **compatible string** defined in the Device Tree. When the kernel finds a DT node whose compatible string matches a registered driver's `of_match_table`, it calls that driver's `probe()` function.

This is different from lab 06 (`wq_poll_driver`) which had no hardware representation — it was a pure software driver with a timer and a buffer. The platform driver here represents real hardware — a GPIO pin on the RPi3.

**Key APIs:**
- `platform_get_irq(pdev, 0)` — reads the IRQ number from the Device Tree node
- `platform_set_drvdata(pdev, data)` — attaches per-device private data to the device
- `module_platform_driver()` — macro that replaces manual `module_init`/`module_exit` for platform drivers

---

## 2. devm_ APIs — Automatic Resource Management

`devm_` prefixed APIs tie resource lifetime to the device lifetime. When `remove()` is called, all `devm_` allocated resources are automatically freed by the kernel.

This means:
- `devm_kzalloc()` — no need to `kfree()` in `remove()`
- `devm_request_irq()` — no need to `free_irq()` in `remove()`

Any resource **not** allocated with `devm_` must be freed manually in `remove()`.

This makes `remove()` very clean — often just a log message when all resources are `devm_` managed.

---

## 3. IRQ Request — Flags

```c
devm_request_irq(&pdev->dev, data->irq, sanath_irq_handler,
                 IRQF_TRIGGER_FALLING, "sanath-irq-lab", data);
```

**`IRQF_TRIGGER_FALLING`** — tells the interrupt controller to fire only on a falling edge (high to low transition). This is correct for an active-low GPIO with pull-up — the pin sits at 3.3V normally and goes to 0V when triggered.

**Last argument `data`** — this is the `dev_id`. It is passed to the IRQ handler as the second argument, giving the handler direct access to the per-device struct (counter, wait queue, IRQ number).

---

## 4. How the CPU Detects Rising vs Falling Edge

Edge detection is done entirely in **hardware by the GPIO controller** (BCM2835), not the CPU itself.

The BCM2835 GPIO controller has dedicated registers:
- **GPREN** — GPIO Rising Edge Detect Enable
- **GPFEN** — GPIO Falling Edge Detect Enable

When `IRQF_TRIGGER_FALLING` is passed to `request_irq()`, the kernel's `pinctrl-bcm2835` driver writes to the **GPFEN register** for GPIO23, enabling falling edge detection in hardware.

The full path when an edge occurs:
```
GPIO23 voltage drops 3.3V → 0V
→ BCM2835 GPIO controller detects falling edge in hardware
→ Sets status bit in GPEDS register
→ Raises interrupt line to GIC (Generic Interrupt Controller)
→ GIC interrupts the CPU
→ CPU saves state, runs your handler
```

The CPU never sees the voltage. It only sees the interrupt signal from the GIC after the hardware has already detected and latched the edge. This is why IRQ handlers are fast — all detection is done before the handler runs.

---

## 5. GPIO Bounce — Physics Affects Code

During testing, interrupts fired continuously even without connecting the jumper wire to GND. A loose wire near GPIO23 acted as an **antenna**, picking up electrical noise from the RPi3's switching regulators and nearby electronics. This noise caused voltage fluctuations that the interrupt controller interpreted as falling edges.

This is called **GPIO bounce** or **floating pin noise**.

**Hardware fix:** RC filter — a 100nF capacitor between GPIO23 and GND filters high frequency noise.

**Software fix:** Minimum time check between interrupts using `jiffies`:
```c
if (now - data->last_irq < msecs_to_jiffies(50))
    return IRQ_HANDLED;
```
Software debounce helps but cannot fully fix physics — noise spikes happen faster than millisecond resolution.

**Key insight:** Unlike application development, kernel driver engineering is hardware-adjacent. Physics directly affects code behaviour. A floating wire, a noisy power supply, or an RC time constant are not abstract concepts — they show up in `dmesg` and `/proc/interrupts`. This is what real embedded engineering looks like.

---

## 6. Why `echo 0 > /sys/class/gpio/gpio23/value` Failed

The DoD required triggering the interrupt via:
```bash
echo 0 > /sys/class/gpio/gpio23/value
```

This failed with `Operation not permitted` because once a driver claims a GPIO via `request_irq()`, the kernel **blocks direct sysfs access** to that pin. The pin is owned exclusively by the driver — no other userspace or kernel path can write to it.

The correct way to trigger the interrupt externally is with a **physical signal** — a jumper wire briefly connecting GPIO23 (Pin 16) to GND (Pin 14). The driver's IRQ handler fires on the falling edge.

Interrupt activity can be verified via:
```bash
cat /proc/interrupts | grep sanath
```

---

## 7. Verifying IRQ in the Kernel

`/proc/interrupts` is more reliable than `dmesg` for counting interrupts since `pr_info` in an IRQ handler can be rate-limited or missed under high interrupt load.

```bash
cat /proc/interrupts | grep sanath
184:        420        466        458        435  pinctrl-bcm2835  23 Edge      sanath-irq-lab
```

Each column is the interrupt count on one CPU core.

---

## 8. Yocto Debugging Lesson

When files disappear from `tmp/work` after a build, always check `local.conf` first:

```bash
grep -i rm_work build-rpi/conf/local.conf
```

`INHERIT += "rm_work"` causes Yocto to delete the work directory after every successful build to save disk space (~30GB). Add active development recipes to `RM_WORK_EXCLUDE` while debugging:

```
RM_WORK_EXCLUDE += "sanath-dtbo"
```

Remove from the exclude list once the recipe is stable.

---

## 9. Spurious Interrupts — Causes and Fixes

During Day 3 testing, multiple unexpected interrupts fired without any deliberate trigger. This prompted a deeper investigation into the different root causes of spurious interrupts and how to distinguish between them.

### Cause 1: Floating Pin (No Pull Resistor)

When a GPIO pin is configured as input with no pull-up or pull-down resistor, it has no defined resting voltage. The pin is connected to the gate of a MOSFET inside the SoC — an extremely high impedance node. With nothing anchoring it to a known voltage, it wanders randomly across the logic threshold (≈1.6V on BCM2835) due to:

- Capacitive coupling from nearby signal traces
- Electromagnetic interference from the environment
- Thermal noise in the MOSFET gate
- Leakage currents in the input protection diodes

The interrupt controller faithfully reports every threshold crossing as a real edge. The driver has no way to distinguish these from real events.

**Fix:** Enable pull-up or pull-down — either external resistor or internal via DT:
```dts
brcm,pull = <2>;   /* 2 = pull-up, 1 = pull-down, 0 = none */
```

In the Day 3 setup, the pull-up was already configured in the DT overlay (`brcm,pull = <2>`), so this was not the cause.

### Cause 2: External Noise Source (Antenna Effect)

Even with the pull-up active, a jumper wire dangling near GPIO23 acted as an antenna. The wire picked up EMI from the RPi3's switching regulators, USB power lines, and nearby traces. This induced enough voltage onto the pin to temporarily overcome the pull-up and cross the falling edge threshold.

This is distinct from a floating pin — the pull-up is working, but a strong enough external disturbance can still win against it. The interrupt controller again sees real edges and fires the handler.

**Key observation:** This happened even before the jumper wire touched GND — the antenna effect alone was sufficient to trigger interrupts.

### Cause 3: Contact Bounce (Mechanical Switch)

When a physical button is pressed, the metal contacts do not make clean contact once. They physically bounce 5–50 times in the first 1–20ms before settling. Each bounce is a real electrical transition — not noise — so the interrupt controller fires on every one.

This is different from the above two causes. The pull-up cannot fix it. Hardware RC filter or software debounce is required.

**Hardware fix:** RC filter — a resistor and capacitor forming a low-pass filter that prevents the voltage from changing faster than the RC time constant (τ = R × C). With R=10kΩ and C=100nF, τ=1ms, which is longer than most bounce windows.

**Software fix:** Timestamp delta check in the IRQ handler. Reject any interrupt that arrives within a debounce window of the last valid one:

```c
static irqreturn_t sanath_irq_handler(int irq, void *dev_id)
{
    struct sanath_irq_data *data = dev_id;
    ktime_t now = ktime_get();

    if (ktime_to_ms(ktime_sub(now, data->last_irq)) < 50)
        return IRQ_HANDLED;  /* inside debounce window — discard */

    data->last_irq = now;
    schedule_work(&data->work);  /* real work goes in workqueue */
    return IRQ_HANDLED;
}
```

**Why `ktime_get()` and not `jiffies`:** On RPi3, `HZ=250` so one jiffy = 4ms. For bounce windows of 1–5ms, jiffies resolution is too coarse. `ktime_get()` gives nanosecond resolution and is safe to call in hardIRQ context.

**Why not `msleep()` in the handler:** Sleeping is forbidden in hardIRQ context. The CPU cannot be scheduled away during interrupt handling. Any sleeping call will deadlock or cause an oops.

**Kernel built-in alternative:** The GPIO subsystem provides a debounce API:
```c
gpiod_set_debounce(gpio_desc, 20000); /* 20000 microseconds = 20ms */
```
Called once in `probe()`. On BCM2835 this falls back to a software implementation since the hardware has no built-in debounce timer. Preferred over manual `ktime` logic as it is more portable.

### Summary Table

| Cause | Pull-up fixes it? | SW debounce fixes it? | HW filter fixes it? |
|---|---|---|---|
| Floating pin (no pull) | ✓ Yes | Partially | Partially |
| Antenna / external EMI | Partially | Partially | ✓ Yes |
| Contact bounce | ✗ No | ✓ Yes | ✓ Yes |

### Interview Answer Template

> "I was seeing spurious interrupts during GPIO testing. My first step was to check the DT overlay — the pull-up was already configured, so floating pin was ruled out. I then noticed the jumper wire was dangling near the board before touching GND — the wire itself was acting as an antenna picking up EMI from the switching regulators. I switched to driving the pin cleanly via sysfs and added software debounce using `ktime_get()` in the handler to reject edges within a 50ms window. The key lesson was that hardware problems and software problems look identical from the driver — spurious interrupts in `dmesg` — so you have to reason about the physical setup before touching code."

---

## Open Questions

- DT overlay syntax — understand how to write from scratch. Revisit on Day 6. Read `Documentation/devicetree/bindings/pinctrl/brcm,bcm2835-gpio.yaml` and study existing RPi overlays in `arch/arm/boot/dts/overlays/`.
- Shared IRQs — revisit when reading CAMSS source on Day 29. Look for `IRQF_SHARED` usage and status register checks.
- U-Boot UART — not available during boot, missing autoboot screen. Investigate U-Boot UART configuration in Yocto.
- Timer race condition — `timer_active` read without lock in `timer_callback()` in lab 06. Revisit and fix when back on that driver.
