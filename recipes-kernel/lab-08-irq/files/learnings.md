# Day 4 Learnings — Lab 08: IRQ + Workqueue Bottom Half
**Date:** April 21, 2026
**Topic:** Top/Bottom Half Split, Workqueue, atomic_t, /dev node with read()

---

## 1. Top/Bottom Half Split

The IRQ handler must be fast and must never sleep. The CPU is in atomic context during hardIRQ — the scheduler cannot preempt it, so any sleeping call will deadlock or cause an oops.

**Rule:**
- IRQ handler — minimal work only. Save timestamp, check debounce, call `schedule_work()`. Nothing else.
- Work function — all real work. Runs in process context, can sleep, can allocate memory, can take mutexes.

The debounce check is safe in the handler because it is pure arithmetic — a comparison of two `jiffies` values. It never blocks and returns immediately either way. `msleep()` by contrast explicitly puts the current context to sleep — forbidden in hardIRQ.

**In sanath-irq-lab:**
```c
static irqreturn_t sanath_irq_handler(int irq, void *dev_id)
{
    struct sanath_irq_data *data = dev_id;
    unsigned long now = jiffies;

    if (now - data->last_irq < msecs_to_jiffies(50))
        return IRQ_HANDLED;  /* debounce — discard */

    data->last_irq = now;
    schedule_work(&data->work);  /* defer everything else */
    return IRQ_HANDLED;
}

static void sanath_irq_work(struct work_struct *work)
{
    struct sanath_irq_data *data = container_of(work, struct sanath_irq_data, work);
    atomic_inc(&data->counter);
    pr_info("sanath-irq-lab: IRQ fired, counter = %d\n", atomic_read(&data->counter));
}
```

---

## 2. DECLARE_WORK vs INIT_WORK

```
DECLARE_WORK   — static, global work item defined at compile time.
                 Work struct lives in global scope.
                 Only one instance exists across all devices.
                 Fine for simple, single-instance use cases.

INIT_WORK      — work struct embedded inside a per-device struct,
                 initialised at runtime in probe().
                 Each device instance gets its own work item.
                 Required when multiple instances of the same driver
                 can exist simultaneously.
```

In sanath-irq-lab, `INIT_WORK` is used because `work_struct` is embedded inside `sanath_irq_data` which is allocated per device in `probe()`. `container_of()` recovers the full struct inside the work function.

```c
/* in probe() */
INIT_WORK(&data->work, sanath_irq_work);

/* in work function */
struct sanath_irq_data *data = container_of(work, struct sanath_irq_data, work);
```

---

## 3. container_of

`container_of(ptr, type, member)` returns the address of the parent struct given a pointer to one of its members.

The work function receives only a `struct work_struct *` pointer. Since `work_struct` is embedded inside `sanath_irq_data`, `container_of` calculates the address of the parent struct by subtracting the known offset of the `work` member from the pointer value.

This is how context (counter, IRQ number, wait queue) is passed to work functions without needing a global or a separate data pointer.

---

## 4. atomic_t vs mutex vs spinlock

```
atomic_t    → single integer, any context
mutex       → multiple variables, process context only
spinlock    → any shared data touched from IRQ context

Single integer?             → atomic_t
Touched from IRQ context?   → spinlock
Process context only?       → mutex
```

In sanath-irq-lab, `atomic_t` is used for the counter because:
- It is a single integer
- `atomic_inc` and `atomic_read` are race-free by design with no locking overhead
- Works in any context — if the counter were ever incremented directly from the IRQ handler, a mutex would be forbidden (atomic context, cannot sleep), but `atomic_t` would still work

---

## 5. char device + platform driver in one module

`module_platform_driver()` macro expands to its own `module_init`/`module_exit` and cannot be used when the module also manages a char device. Instead, use explicit `module_init`/`module_exit` and register the platform driver manually:

```c
static int __init etx_driver_init(void)
{
    /* char device setup: alloc_chrdev_region, cdev_init, cdev_add,
       class_create, device_create */

    return platform_driver_register(&sanath_irq_driver);
}

static void __exit etx_driver_exit(void)
{
    platform_driver_unregister(&sanath_irq_driver);
    /* char device cleanup: device_destroy, class_destroy,
       cdev_del, unregister_chrdev_region */
}
```

---

## 6. etx_read — offset handling and EOF

When userspace calls `cat /dev/sanath_queue`, the C library calls `read()` in a loop until it receives `0` — which signals EOF. Without offset tracking, `read()` returns data every call and `cat` loops forever.

`*off` tracks how many bytes have already been read. After returning data, advance `*off` by bytes returned. On the next call, `*off > 0` — return 0 — cat sees EOF and exits.

```c
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[32];
    int kbuf_len;

    if (*off > 0)
        return 0;  /* EOF */

    if (!g_data)
        return -ENODEV;

    kbuf_len = snprintf(kbuf, sizeof(kbuf), "%d\n", atomic_read(&g_data->counter));

    if (copy_to_user(buf, kbuf, kbuf_len))
        return -EFAULT;

    *off += kbuf_len;
    return kbuf_len;
}
```

---

## 7. cancel_work_sync in remove()

If `rmmod` is called while work is pending or running, the work function may access `data->counter` after `devm_` has already freed `sanath_irq_data` — a use-after-free, which causes a kernel oops.

`cancel_work_sync(&data->work)` blocks in `remove()` until any pending or in-flight work completes. This guarantees the data struct remains valid for the entire duration of the work function before it is freed.

```c
static int sanath_irq_remove(struct platform_device *pdev)
{
    struct sanath_irq_data *data = platform_get_drvdata(pdev);
    cancel_work_sync(&data->work);
    dev_info(&pdev->dev, "sanath-irq-lab removed\n");
    return 0;
}
```

---

## Open Questions

- `*off` is per file descriptor — what happens if two processes open `/dev/sanath_queue` simultaneously and both call `read()`? Each has its own `*off` so both get the counter value independently. Is this the correct behaviour for a counter device?
- `g_data` is a global pointer set in `probe()`. If `rmmod` is called while `etx_read()` is executing, `g_data` could be freed mid-read. This is a use-after-free risk. Revisit when studying driver reference counting and `file->private_data` pattern.
