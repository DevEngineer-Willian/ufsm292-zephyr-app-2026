#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise production KSZ8851SNL functions with deterministic host fault injection."""
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
DRIVER = ROOT / "drivers/ethernet/eth_ksz8851snl.c"
HERE = pathlib.Path(__file__).resolve().parent
FUNCTIONS = (
    "request_recovery_locked", "rx_drop", "rx_release", "rx_drain",
    "wait_metfe_clear", "configure_qmu_locked", "recover", "worker", "send",
)


def function_body(source, name):
    match = re.search(r"(?m)^static\s+(?:void|int)\s+" + name + r"\s*\(", source)
    if not match:
        raise RuntimeError(f"production function not found: {name}")
    start = source.find("{", match.end())
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():index + 1]
    raise RuntimeError(f"unterminated production function: {name}")


source = DRIVER.read_text()
constants = source[source.index("#define KS_CCR"):source.index("struct ksz_debug")]
production = "\n\n".join(function_body(source, name) for name in FUNCTIONS)
harness = (HERE / "harness.c.in").read_text().replace("/* DRIVER_CONSTANTS */", constants)
with tempfile.TemporaryDirectory(prefix="ksz8851snl-") as tmp:
    test_c = pathlib.Path(tmp) / "test.c"
    exe = pathlib.Path(tmp) / "test"
    test_c.write_text(harness + "\n" + production + "\n")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O0",
                    str(test_c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
