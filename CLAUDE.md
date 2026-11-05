# Claude Code Project Notes

## Lab Operations (labctl)

Hardware interaction with the STM32MP157F-DK2 is managed through **labctl**, a centralized lab management system available as an MCP server. The labctl MCP server is configured globally in `~/.claude/settings.json` and provides tools for power control, health checks, and SBC management.

The SBC is registered as **`stm32mp157f-dk2`** in labctl.

### Power Management

Power control is the primary lab operation for this project. Use the labctl MCP tools:

| Operation | MCP Tool | Parameters |
|-----------|----------|------------|
| Power on | `power_on` | `sbc_name: "stm32mp157f-dk2"` |
| Power off | `power_off` | `sbc_name: "stm32mp157f-dk2"` |
| Power cycle | `power_cycle` | `sbc_name: "stm32mp157f-dk2"` |
| Check health | `run_health_check` | `sbc_name: "stm32mp157f-dk2"` |

The board is powered via a Kasa smart plug (HS300 strip, index 5) at `192.168.4.140`.

**Prefer software reboot when possible:**
```bash
ssh root@smarthub.local reboot
```

Use power cycle only when SSH is unresponsive or the system is hung.

### Network Access

```bash
ssh root@smarthub.local          # mDNS (preferred)
ssh root@192.168.4.99            # Ethernet
ssh root@192.168.4.67            # WiFi
```

### Serial and SDWire

Serial console and SDWire are **not currently assigned** for this SBC in labctl. If needed in the future, they can be added via `labctl port assign` and `labctl sdwire assign`. See the [labctl documentation](https://github.com/johnjezl/Embedded-Lab-Control) for details.

---

## Building the Application

### Build System

The project uses **CMake with Ninja** as the build system (not Make).

**ARM build (for target device):**
```bash
cd app/build-arm
ninja            # Build all
ninja smarthub   # Build just the main binary
```

**Native build (for testing):**
```bash
cd app/build-test
ninja
./tests/test_zigbee  # Run specific test
```

**Important:** Do NOT use `make` - it will produce no output because the build uses Ninja. Always use `ninja` instead.

### Deploying to Device

```bash
# Transfer binary
cat app/build-arm/smarthub | ssh root@smarthub.local "cat > /usr/bin/smarthub && chmod +x /usr/bin/smarthub"

# Restart the application
ssh root@smarthub.local "killall smarthub; nohup smarthub > /var/log/smarthub.log 2>&1 &"
```

If the board is unresponsive, use the `power_cycle` MCP tool with `sbc_name: "stm32mp157f-dk2"` before deploying.

---

### TODO File Formatting

Use emoji markers for task status:

| Status | Marker | Example |
|--------|--------|---------|
| Completed | ✅ | `- ✅ Task completed` |
| Pending | ☐ | `- ☐ Task pending` |
| Blocked | 🔗 | `- ☐🔗 Task — requires M4` |
| Deferred | ⏸️ | `- ⏸️ Task — deferred to Phase 4` |

**Definitions:**
- **Pending** ☐ — Ready to work on now
- **Blocked** 🔗 — Waiting on dependency within this phase (combine with ☐ or ⏸️)
- **Deferred** ⏸️ — Postponed to a future phase (e.g., "deferred to Phase 5")

**Correct:**
```markdown
- ✅ Task completed
- ☐ Task pending
- ☐🔗 Task — requires M4 (pending, has dependency)
- ⏸️ Task — deferred to Phase 5
- ⏸️🔗 Task — deferred, had dependency when deferred
```

**Incorrect:**
```markdown
- [x] Task completed  ← Don't use this
- [ ] Task pending    ← Use ☐ instead
- ✓ Task completed    ← Don't use plain check symbol
- Deferred: Task      ← Use ⏸️ emoji instead
```
