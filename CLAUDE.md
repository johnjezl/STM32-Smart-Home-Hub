# Claude Code Project Notes

## Lab Operations (labctl)

Hardware interaction with the STM32MP157F-DK2 is managed through **labctl**, a centralized lab management system available as an MCP server, configured in this repo's `.mcp.json`. It provides tools for power control, health checks, and SBC management.

**Only use labctl through its MCP tools. Never run the `labctl` CLI**, not even for read-only commands. If the tools aren't available in a session, get the MCP server loaded instead of falling back to the CLI.

**labctl is the source of truth for the lab.** Which SBC is this project's board, its addresses, its power plug, and any serial console or SDWire assignment all live there, not in this repo. Look them up each time: the `lab://sbcs` resource lists every SBC with its project (`SmartHub`), name, SSH user and addresses. Don't add lab details to this repo.

### Power Management

Use the labctl MCP tools, with the board's SBC name from labctl:

| Operation | MCP Tool |
|-----------|----------|
| Power on | `power_on` |
| Power off | `power_off` |
| Power cycle | `power_cycle` |
| Check health | `run_health_check` |

Claim the board with `claim_sbc` before working on it, and release it with `release_sbc` when done.

**Prefer software reboot when possible:**
```bash
ssh root@<board-ip> reboot
```

Use power cycle only when SSH is unresponsive or the system is hung.

### Network Access

```bash
ssh root@<board-ip>              # An address from labctl
ssh root@smarthub.local          # mDNS, if this machine resolves .local names
```

### Serial and SDWire

If labctl assigns the board a serial console or an SDWire, its MCP tools use them (`serial_capture`, `serial_send`, the `sdwire_*` tools). See the [labctl documentation](https://github.com/johnjezl/Embedded-Lab-Control) for details.

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
# Sends the file over ssh stdin because scp may fail against the board's Dropbear. Don't use `cat file | ssh`:
# if the file is missing, the pipeline would still install an empty binary. Restarts only if the install succeeded.
ssh root@<board-ip> "cat > /usr/bin/smarthub.new && chmod +x /usr/bin/smarthub.new && mv -f /usr/bin/smarthub.new /usr/bin/smarthub" < app/build-arm/smarthub && \
  ssh root@<board-ip> "killall smarthub; for i in 1 2 3 4 5 6 7 8 9 10; do pidof smarthub > /dev/null || break; sleep 1; done; nohup smarthub > /var/log/smarthub.log 2>&1 < /dev/null &"
```

`smarthub` on the board's PATH is `/usr/bin/smarthub`, so install there. The board also has older copies in `/opt/smarthub/bin` and `/root`. `tools/deploy.sh` does all this, and the dashboard too.

An image built from this repository has `/etc/init.d/S90smarthub`, which starts the app and restarts it if it crashes. On one of those, use `/etc/init.d/S90smarthub restart` rather than the `killall` above, or the init script loses track of it.

If the board is unresponsive, use the `power_cycle` MCP tool before deploying.

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

1. Work on a branch, get CI green, and validate on the board (this project's SBC in labctl). Runs on an x86-64 host don't count as validation. Only changes that can't affect the board, such as docs and tooling config, skip the board.
2. Open the PR, then run a code review of it.
3. Address all actionable findings, and review again. Repeat until the review is clean.
4. Merge, then go on to the next ticket. Stop only for a roadblock, or if the loop runs too long.

Don't treat green CI as approval to merge.
