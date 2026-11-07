#!/usr/bin/env python3
#
# Tiku Operating System v0.06
# Simple. Persistent. Intelligent.
# http://tiku-os.org
#
# Authors: Ambuj Varshney <ambuj@tiku-os.org>
#
# translate.py - map pinned vendor assembly onto the P4 instruction macros.
#
# SPDX-License-Identifier: Apache-2.0
"""Translate supported ESP mnemonics and refuse unknown instructions."""
import argparse
from pathlib import Path
import re


def translate(source, root):
    """Return upstream-binutils assembly using the verified instruction macros."""
    inc = root / "arch/esp32p4/pie/tiku_pie.inc"
    known = set(re.findall(r"^\.macro (\w+)", inc.read_text(), re.M))

    def instruction(match):
        mnemonic = match[0]
        if mnemonic == "esp.lp.setup":
            return "tiku_p4_lp_setup"
        macro = mnemonic.replace(".", "_")
        if macro not in known:
            raise ValueError("unsupported ESP-NN instruction: " + mnemonic)
        return macro

    source = re.sub(r"\besp\.[a-z0-9_.]+", instruction, source)
    return (f'.include "{inc}"\n'
            f'.include "{root}/arch/esp32p4/pie/tiku_hwloop.inc"\n'
            '.option norelax\n' + source)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(translate(args.input.read_text(), args.root.resolve()))
