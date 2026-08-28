#!/usr/bin/env python3
"""Generate Figure 15(a) from the slot-completion-time samples."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def ecdf(values):
    x = np.sort(np.asarray(values, dtype=float))
    return x, np.arange(1, len(x) + 1) / len(x)


def main():
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=here / "cdf_slot_completion_time.pdf")
    args = parser.parse_args()

    groups = {
        "Without Chronos": ["nc_connnect", "nc_iperfdl", "nc_iperful", "nc_ping"],
        "With Chronos": ["c_connect", "c_iperfdl", "c_iperful", "c_ping"],
    }
    fig, ax = plt.subplots(figsize=(5.2, 3.2))
    for label, names in groups.items():
        samples = np.concatenate([np.loadtxt(here / name, ndmin=1) for name in names])
        x, y = ecdf(samples)
        ax.plot(x, y, linewidth=2.2, label=label)
    ax.set_xlabel("Slot completion time (µs)")
    ax.set_ylabel("CDF")
    ax.grid(True, alpha=0.3)
    ax.legend(framealpha=0.7)
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
