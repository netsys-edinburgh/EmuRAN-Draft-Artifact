# EmuRAN Artifact (Draft)

This repository contains the draft artifact for **EmuRAN: Cloud based RAN
Digital Twin**. EmuRAN is a cloud-deployable RAN emulation system built around
four ideas described in Sections 4 and 5 of the paper:

1. Bypass the compute- and bandwidth-intensive PHY layer using nFAPI.
2. Virtualize time so cloud preemptions and network delays do not cause RAN
   deadlines to expire inside the emulation.
3. Advance all components through RAN slots using distributed synchronization
   barriers.
4. Distribute the middlebox that forwards control- and data-plane traffic
   between gNBs and UEs.

The artifact is organized into source code, deployment automation, and data and
scripts for selected evaluation results.

## Repository layout

| Directory | Contents |
| --- | --- |
| [`code/`](code/) | EmuRAN's virtual-time, synchronization, middlebox, gNB, and UE implementations. |
| [`deployment/`](deployment/) | Infrastructure provisioning scripts and Helm charts for deploying experiments. |
| [`results/`](results/) | Raw logs, packet captures, processed data, and plotting scripts for selected paper results. |

## `code/`: core components

| Path | Role |
| --- | --- |
| [`code/supervisor/`](code/supervisor/) | Patched Linux 5.15 kernel source used by the EmuRAN supervisor. The KVM/VMX path serves guest `RDTSC` reads from the virtual TSC exported by the EmuRAN clock module, allowing guest time to be advanced or paused independently of wall-clock time. |
| [`code/slot_monitor/`](code/slot_monitor/) | The `custom_tsc` kernel module and user-space shared-memory/slot-monitor utilities. The module advances the virtual TSC with a high-resolution timer and exposes the control path used to pause time at a slot boundary and resume it for the next slot. The directory also contains development variants and test programs. |
| [`code/middlebox/`](code/middlebox/) | Modified multi-UE nFAPI proxy. It routes PHY-bypass traffic between gNBs and UEs, tracks local slot completion, communicates with the global slot coordinator, and supports distributing the forwarding load across proxy instances. |
| [`code/gnb/`](code/gnb/) | Bug-fixed OpenAirInterface gNB source tree. Deployments run it in nFAPI VNF and emulated-L1 mode so the real MAC-and-above stack can operate without a full PHY. |
| [`code/ue/`](code/ue/) | Customized OpenAirInterface UE source tree. Deployments run it in standalone nFAPI PNF and emulated-L1 mode, with one or more UEs connected through the middlebox. |

At runtime, the local slot monitor stops virtual time after a slot has elapsed.
The middlebox and other participating components report completion to the global
slot coordinator. Once every active component has completed the current slot,
the coordinator releases the next slot and the local clocks resume. This trades
wall-clock execution time for emulation fidelity when the underlying cloud
resources are delayed or oversubscribed.

The `supervisor`, `gnb`, and `ue` directories are complete source trees rather
than small patch sets, so they account for most of the repository size.

## `deployment/`: provisioning and experiment deployment

### Infrastructure provisioning

[`deployment/provisioning/`](deployment/provisioning/) contains
node-configuration scripts that are mostly generic to Ubuntu, plus a CloudLab
profile. The Ubuntu scripts:

- build and install the patched kernel and `custom_tsc` module;
- configure CPU isolation, networking, routes, storage, and SCTP;
- create the nested Ubuntu/KVM guests used for emulated components; and
- install and join the Kubernetes cluster used to run an experiment.

[`deployment/provisioning/profile.py`](deployment/provisioning/profile.py)
automates the CloudLab deployment: it allocates the controller, compute, proxy,
and global-slot-coordinator nodes and runs the appropriate provisioning scripts
on them. Additional provisioning details are in
[`deployment/provisioning/README.md`](deployment/provisioning/README.md).

### Kubernetes deployment

[`deployment/helm/`](deployment/helm/) contains the experiment configuration,
Helm charts, container build files, and operational helper scripts:

| Chart | Deployed component |
| --- | --- |
| `core/` | Open5GS core and subscriber database |
| `globalsc/` | Global slot coordinator |
| `proxy/` | Distributed EmuRAN middleboxes |
| `gnb/` | OAI gNB instances |
| `ue/` | OAI UE instances and data-plane measurement sidecars |
| `persistue/` | Persistent UE support used by selected experiments |

[`deployment/helm/values.yaml`](deployment/helm/values.yaml) is the central
configuration for topology size, node counts, images, addressing, startup
behavior, and data-plane workload. The helper scripts collect logs, restart or
inspect UEs, and update experiment parameters.

The checked-in defaults describe a very large experiment (10,000 gNBs and
10,000 UEs). Review all values before deploying. The current setup also assumes
privileged containers, nested virtualization, fixed private address ranges,
specific node names, and externally hosted container images.

## `results/`: evaluation data and plots

This directory preserves selected evidence for the fidelity, scalability, and
subsystem experiments in Section 6 of the paper.

| Path | Experiment or output |
| --- | --- |
| `LargeScale/` | EmuRAN and EMANE scale sweeps, including per-UE control/data-plane logs, core logs, global coordinator logs, and Kubernetes snapshots. |
| `EMANE/` | Additional raw EMANE scale-run logs. |
| `fig9a/` | Data-plane RTT as the number of emulated gNB/UE pairs increases. |
| `fig9b/` | Fraction of attempted UEs that establish a working data plane as scale increases. |
| `fig9c/` | Global slot-coordinator arrival spread and dilation-factor analysis. |
| `fig9d/` | CPU cost of splitting the middlebox load across proxy instances. |
| `fig10a/` | Packet-capture analysis of per-slot middlebox processing/wait time at different loads. |
| `fig10b/` | EmuRAN-versus-EMANE data-plane RTT on the testbed and Azure. |
| `fig10c/` | Effect of induced compute preemptions on UE RTT and the dilation factor. |
| `fig10d/` | Effect of induced network delay on UE RTT and the dilation factor. |

Each experiment directory contains the relevant combination of raw inputs,
analysis scripts, intermediate CSV files, and generated PDF plots.
