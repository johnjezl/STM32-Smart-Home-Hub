# Claude Code Project Notes

## Serial Console Access

### Important: Cygwin Environment Setup

Claude Code runs in Git Bash, which has different mount points than Cygwin. When running Cygwin bash from Claude Code, the inherited Git Bash environment causes `/usr/bin` to point to Git Bash's binaries instead of Cygwin's. This makes Cygwin-installed programs like `picocom` invisible.

**Solution:** Always use `env -i` to clear the inherited environment before running Cygwin:

```bash
# CORRECT: Clean environment - picocom will be found
C:/cygwin64/bin/env.exe -i HOME=/tmp PATH=/usr/bin:/bin C:/cygwin64/bin/bash.exe --login -c "picocom -b 115200 /dev/ttyS7"

# WRONG: Inherits Git Bash mounts - picocom not found
C:/cygwin64/bin/bash.exe --login -c "picocom -b 115200 /dev/ttyS7"
```

**Verification:** Check which `/usr/bin` is active:

```bash
C:/cygwin64/bin/env.exe -i PATH=/usr/bin:/bin C:/cygwin64/bin/bash.exe -c "mount | grep usr"
# Should show: C:/cygwin64/bin on /usr/bin
# NOT: C:/Program Files/Git/usr/bin on /usr/bin
```

### Serial Port Mapping

| Cygwin Device | Windows Port | Connection |
|---------------|--------------|------------|
| /dev/ttyS7 | COM8 | ST-LINK serial port |

### Interactive Serial Session

To open an interactive serial console to the STM32MP157F-DK2:

```bash
C:/cygwin64/bin/env.exe -i HOME=/tmp PATH=/usr/bin:/bin C:/cygwin64/bin/bash.exe --login -c "picocom -b 115200 /dev/ttyS7"
```

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
