# Chronos Artifact

This is the artifact for **Chronos: Scalable and Cost-Effective Cloud-based RAN
Emulation System**.

Chronos is a cloud-deployable RAN emulation system built around four ideas:

1. Bypass the compute- and bandwidth-intensive PHY layer using nFAPI.
2. Virtualize time so cloud preemptions and network delays do not cause RAN
   deadlines to expire inside the emulation.
3. Advance components through RAN slots using distributed synchronization
   barriers.
4. Distribute the middlebox that forwards control- and data-plane traffic
   between gNBs and UEs.

The artifact provides the implementation, Powder/Emulab deployment automation,
experiment data, scripts used to generate the paper plots, prepared PDF
outputs, and access to a hosted Powder evaluation deployment.

## Badge scope

The artifact supports:

- **Artifacts Available:** the source, deployment scripts, experiment data, and
  plotting workflow are publicly available in this repository.
- **Artifacts Evaluated—Functional:** evaluators can inspect a running Chronos
  deployment on Powder and verify end-to-end operation.
- **Results Reproduced:** evaluators can regenerate the paper plots from the
  supplied data and reproduce the scale-dependent dilation result on Powder.

## Clone the repository

```bash
git clone https://github.com/netsys-edinburgh/EmuRAN-Draft-Artifact.git
cd EmuRAN-Draft-Artifact
```

## Repository layout

| Path | Contents |
| --- | --- |
| [`code/`](code/) | Chronos virtual-time, synchronization, middlebox, gNB, and UE implementations. |
| [`deployment/`](deployment/) | Powder/Emulab provisioning scripts, Kubernetes Helm charts, and operational scripts. |
| [`results/data/`](results/data/) | Raw logs, packet captures, processed data, helper modules, and plotting scripts. |
| [`results/output/`](results/output/) | Prepared and newly generated PDF plots. |
| [`plot.ipynb`](plot.ipynb) | One plotting cell per paper figure or panel. |
| [`requirement.txt`](requirement.txt) | Python dependencies for the plotting workflow. |
| [`install_dependencies.sh`](install_dependencies.sh) | Linux/macOS dependency installer. |
| [`ae.tex`](ae.tex) / [`ae.pdf`](ae.pdf) | Full artifact-evaluation instructions. |

## Run the plotting notebook

### Requirements

- Linux or macOS
- Python 3.10 or newer
- Internet access for the initial dependency installation
- No GPU, LaTeX installation, or privileged access is required

Install all dependencies into a local `.venv`:

```bash
./install_dependencies.sh
```

The installer uses `requirement.txt` and installs Jupyter, IPython, IPykernel,
NumPy, Pandas, Matplotlib, Seaborn, and PyMuPDF. It also registers the dedicated
`Python (Chronos Artifact)` notebook kernel. To override the Python executable
or environment location:

```bash
PYTHON_BIN=/path/to/python3 VENV_DIR=/path/to/venv ./install_dependencies.sh
```

Launch the notebook from the repository root:

```bash
source .venv/bin/activate
jupyter notebook plot.ipynb
```

If Jupyter prompts for a kernel, select `Python (Chronos Artifact)`. This makes
the notebook use the environment containing PyMuPDF and the other plotting
dependencies.

Each code cell:

1. runs the corresponding plotting script under `results/data/`;
2. writes a fresh PDF to `results/output/`; and
3. displays that newly generated PDF inline.

Run one cell for one figure, or select **Run All**. A complete run produces 28
PDF files in `results/output/`.

The complete notebook can also be executed non-interactively:

```bash
source .venv/bin/activate
jupyter nbconvert --to notebook --execute plot.ipynb \
  --output plot.executed.ipynb --ExecutePreprocessor.timeout=300
find results/output -maxdepth 1 -name '*.pdf' | wc -l
```

The expected PDF count is `28`. The notebook uses paths relative to the
repository root, so it can be cloned and run without editing machine-specific
paths.

## Hosted Powder evaluation

The authors provide a running Chronos deployment on Powder from **September 1
through September 15**.

### Request access

Post your **public SSH key** in a comment on the paper's HotCRP
artifact-evaluation page. The authors will add the key to the deployment and
reply through HotCRP with the SSH command, hostname, username, and any required
jump-host instructions.

Only send the public key, such as the contents of `id_ed25519.pub`. Keep the
matching private key, passphrase, passwords, and access tokens private.

After receiving the connection details, connect using the command provided in
HotCRP. Its general form is:

```bash
ssh -i <private-key> <user>@<host>
```

## Run Chronos and reproduce scale-dependent dilation

The hosted Powder experiment reproduces a key paper result: as the number of
emulated gNB/UE pairs increases on a fixed compute allocation, Chronos increases
its dilation factor. This gives components additional wall-clock time per unit
of virtual time and preserves coordinated slot execution when compute becomes
constrained.

After connecting to `node0` using the SSH command supplied through HotCRP,
connect from `node0` to the controller and enter the deployment directory:

```bash
ssh ubuntu@10.2.1.2
cd chronos-auto-deploy
```

For each scale in `1`, `10`, `50`, `100`, `200`, and `300`:

1. Open `values.yaml` and set both `numberGNB` and `numberUE` to the same
   scale value.
2. Run `./run_experiment.sh` to deploy Chronos.
3. In the interactive interface, press `p` to display the pods. Wait until all
   expected pods are running.
4. Run `./core-logs.sh` and confirm that all gNBs and UEs are connected.
5. In the interactive interface, enter `l globalsc` to display the global
   coordinator log. Observe and record the dilation value.
6. Run `./teardown-experiment.sh` and wait for the deployment to be removed.
7. Change both scale values in `values.yaml` to the next value and repeat.

Do not start the next scale until the previous deployment has been torn down.
A dilation factor near `1` means that the emulation is keeping pace with real
time; a value above `1` means Chronos allocates additional wall-clock time to
complete each unit of virtual time.

The result is reproduced when dilation remains near real time while compute
capacity is sufficient and then rises as increasing gNB/UE scale creates
compute pressure. Record the value observed at every scale; the reproduced
result is the increasing dilation trend while coordinated slot execution
continues.

## Source-code components

| Path | Role |
| --- | --- |
| [`code/supervisor/`](code/supervisor/) | Patched Linux 5.15/KVM source that serves guest `RDTSC` reads from Chronos's virtual TSC. |
| [`code/slot_monitor/`](code/slot_monitor/) | `custom_tsc` module and slot-monitor utilities controlling virtual time. |
| [`code/middlebox/`](code/middlebox/) | Distributed multi-UE nFAPI proxy and slot-completion logic. |
| [`code/gnb/`](code/gnb/) | Modified OpenAirInterface gNB running in nFAPI VNF/emulated-L1 mode. |
| [`code/ue/`](code/ue/) | Modified OpenAirInterface UE running in standalone nFAPI PNF/emulated-L1 mode. |

At runtime, local slot monitors stop virtual time at slot boundaries. The
middleboxes and participating components report completion to the global slot
coordinator. Once all active components complete the slot, the coordinator
releases the next slot and local virtual clocks resume.

## Deployment automation

[`deployment/provisioning/profile.py`](deployment/provisioning/profile.py) is
the Powder/Emulab profile. It allocates controller/compute, proxy, and global
slot-coordinator nodes and invokes the role-specific provisioning scripts.

[`deployment/provisioning/scripts/`](deployment/provisioning/scripts/) builds
and installs the patched kernel, configures networking and SCTP, creates nested
Ubuntu/KVM guests, installs virtual-time services, and constructs the
Kubernetes cluster.

[`deployment/helm/`](deployment/helm/) contains charts for:

| Chart | Component |
| --- | --- |
| `core/` | Mobile core and subscriber database |
| `globalsc/` | Global slot coordinator |
| `proxy/` | Distributed Chronos middleboxes |
| `gnb/` | OAI gNB instances |
| `ue/` | OAI UE instances and data-plane measurement sidecars |
| `persistue/` | Persistent UE support used by selected experiments |

The central configuration file is
[`deployment/helm/values.yaml`](deployment/helm/values.yaml). Operational
scripts in the same directory start UEs, inspect or restart components, change
scale parameters, and collect logs.

## More information

See [the artifact document](ae.pdf) for the complete evaluation procedure and
access details. Additional provisioning notes are available in
[`deployment/provisioning/README.md`](deployment/provisioning/README.md).
