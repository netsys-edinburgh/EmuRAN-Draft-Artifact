#!/usr/bin/env python3
"""Generate Figure 15(b) from the iperf3 logs."""

import argparse
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

RATE = re.compile(r"([0-9.]+)\s+([KMG])bits/sec")
SCALE = {"K": 1e-3, "M": 1.0, "G": 1e3}


def rates(path):
    values = []
    for line in path.read_text(errors="ignore").splitlines():
        if "sender" in line or "receiver" in line:
            continue
        match = RATE.search(line)
        if match:
            values.append(float(match.group(1)) * SCALE[match.group(2)])
    if not values:
        raise ValueError(f"No interval rates found in {path}")
    return values


def main():
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=here / "iperf3_speeds_comparison.pdf")
    args = parser.parse_args()

    series = {
        "DL without Chronos": rates(here / "iperf_nc_dl"),
        "DL with Chronos": rates(here / "iperf3_c_dl"),
        "UL without Chronos": rates(here / "iperf3_nc_ul"),
        "UL with Chronos": rates(here / "iperf3_c_ul"),
    }
    labels = list(series)
    means = [np.mean(series[label]) for label in labels]
    errors = [np.std(series[label]) for label in labels]
    colors = ["tab:blue", "tab:orange", "tab:blue", "tab:orange"]
    fig, ax = plt.subplots(figsize=(6.2, 3.2))
    ax.bar(range(len(labels)), means, yerr=errors, capsize=3, color=colors, edgecolor="black")
    ax.set_xticks(range(len(labels)), ["DL\nNo Chronos", "DL\nChronos", "UL\nNo Chronos", "UL\nChronos"])
    ax.set_ylabel("Throughput (Mbps)")
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
