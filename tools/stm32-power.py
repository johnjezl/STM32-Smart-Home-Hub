#!/usr/bin/env python3
"""
STM32 Power Control via Kasa Smart Plug

Usage:
    stm32-power.py [on|off|cycle|status] [--host IP]

Examples:
    stm32-power.py status
    stm32-power.py off
    stm32-power.py on
    stm32-power.py cycle          # off, wait 3s, on
    stm32-power.py status --host 192.168.4.100
"""

import asyncio
import argparse
import sys
from kasa import Device

DEFAULT_HOST = "192.168.4.96"
CYCLE_DELAY = 3  # seconds between off and on


async def get_device(host: str) -> Device:
    dev = await Device.connect(host=host)
    await dev.update()
    return dev


async def cmd_status(host: str):
    dev = await get_device(host)
    state = "ON" if dev.is_on else "OFF"
    print(f"Device: {dev.alias}")
    print(f"Host:   {host}")
    print(f"State:  {state}")


async def cmd_on(host: str):
    dev = await get_device(host)
    if dev.is_on:
        print("Already ON")
    else:
        await dev.turn_on()
        print("Turned ON")


async def cmd_off(host: str):
    dev = await get_device(host)
    if not dev.is_on:
        print("Already OFF")
    else:
        await dev.turn_off()
        print("Turned OFF")


async def cmd_cycle(host: str):
    dev = await get_device(host)
    print("Power cycling...")
    await dev.turn_off()
    print(f"OFF - waiting {CYCLE_DELAY}s...")
    await asyncio.sleep(CYCLE_DELAY)
    await dev.turn_on()
    print("ON")


def main():
    parser = argparse.ArgumentParser(description="STM32 Power Control via Kasa Smart Plug")
    parser.add_argument("command", nargs="?", default="status",
                        choices=["on", "off", "cycle", "status"],
                        help="Command to execute (default: status)")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"Kasa plug IP address (default: {DEFAULT_HOST})")

    args = parser.parse_args()

    commands = {
        "status": cmd_status,
        "on": cmd_on,
        "off": cmd_off,
        "cycle": cmd_cycle,
    }

    try:
        asyncio.run(commands[args.command](args.host))
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
