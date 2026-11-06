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
ssh root@192.168.4.102 reboot
```

Use power cycle only when SSH is unresponsive or the system is hung.

### Network Access

```bash
ssh root@192.168.4.102           # Ethernet (preferred)
ssh root@192.168.4.99            # WiFi
ssh root@smarthub.local          # mDNS, if this machine resolves .local names
```

These are DHCP leases and they do move. If one stops answering, check with `ip -4 addr` on the board. The addresses in the labctl inventory may be out of date.

### Serial and SDWire

Serial console and SDWire are **not currently assigned** for this SBC in labctl. Ask the maintainer if you need them. See the [labctl documentation](https://github.com/johnjezl/Embedded-Lab-Control) for details.

**Never use a bare `cat` on a tty device** such as `/dev/ttyACM0`. It waits for an EOF that never comes and hangs the session. Use `timeout` or `picocom`.

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
# Install under a temp name, then rename: this works while the old binary is running.
# Uses a cat pipe because scp may fail against the board's Dropbear. Restarts only if the install succeeded.
cat app/build-arm/smarthub | ssh root@192.168.4.102 "cat > /usr/bin/smarthub.new && chmod +x /usr/bin/smarthub.new && mv -f /usr/bin/smarthub.new /usr/bin/smarthub" && \
  ssh root@192.168.4.102 "killall smarthub; for i in 1 2 3 4 5 6 7 8 9 10; do pidof smarthub > /dev/null || break; sleep 1; done; nohup smarthub > /var/log/smarthub.log 2>&1 < /dev/null &"
```

`smarthub` on the board's PATH is `/usr/bin/smarthub`, so install there. The board also has older copies in `/opt/smarthub/bin` and `/root`, and `tools/deploy.sh` installs to `/opt/smarthub`, which this command doesn't run.

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
