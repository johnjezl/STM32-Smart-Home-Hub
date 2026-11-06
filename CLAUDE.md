# Claude Code Project Notes

## Lab Operations (labctl)

Hardware interaction with the STM32MP157F-DK2 is managed through **labctl**, a centralized lab management system available as an MCP server, configured in this repo's `.mcp.json`. It provides tools for power control, health checks, and SBC management.

**Only use labctl through its MCP tools. Never run the `labctl` CLI**, not even for read-only commands. If the tools aren't available in a session, get the MCP server loaded instead of falling back to the CLI.

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
ssh root@192.168.4.99 reboot
```

Use power cycle only when SSH is unresponsive or the system is hung.

### Network Access

```bash
ssh root@192.168.4.99            # Ethernet (preferred)
ssh root@192.168.4.67            # WiFi
ssh root@smarthub.local          # mDNS, if this machine resolves .local names
```

### Serial and SDWire

Serial console and SDWire are **not currently assigned** for this SBC in labctl. Ask the maintainer if you need them. See the [labctl documentation](https://github.com/johnjezl/Embedded-Lab-Control) for details.

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

**Native build (quick host checks only, not validation; see Pull Requests):**
```bash
cd app/build-test
ninja
./tests/test_zigbee  # Run specific test
```

**Important:** Do NOT use `make` - it will produce no output because the build uses Ninja. Always use `ninja` instead.

### Deploying to Device

```bash
# Stop the application first: a running binary can't be overwritten
ssh root@192.168.4.99 "killall smarthub; sleep 1"

# Transfer binary
cat app/build-arm/smarthub | ssh root@192.168.4.99 "cat > /usr/bin/smarthub && chmod +x /usr/bin/smarthub"

# Start the application
ssh root@192.168.4.99 "nohup smarthub > /var/log/smarthub.log 2>&1 &"
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

---

## Pull Requests

Every PR gets a code review **before** it merges, and fast-forwarding `main` counts as merging.

1. Work on a branch, get CI green, and validate on the board (`stm32mp157f-dk2`). Runs on an x86-64 host don't count as validation. Only changes that can't affect the board, such as docs and tooling config, skip the board.
2. Open the PR, then run a code review of it.
3. Address all actionable findings, and review again. Repeat until the review is clean.
4. Merge, then go on to the next ticket. Stop only for a roadblock, or if the loop runs too long.

Don't treat green CI as approval to merge.
