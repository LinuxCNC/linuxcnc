# MendelMax CRAMPS configuration for Machinekit

3D printer configuration using Machinekits Python API for my MendelMax.

This configuration does not use the PRU pin hunting fix. If you want
enable this please take a look at the Fabrikator-Mini-CRAMPS
configuration.

The configuration uses `mkwrapper` to service the [Machineface](https://github.com/qtquickvcp/Machineface) UI
from the `~/Machineface` directory.

## Setup
To get started download the [Machineface](https://github.com/qtquickvcp/Machineface) UI to your home directory:

```bash
cd
git clone https://github.com/qtquickvcp/Machineface.git
```

Then download and install the [MachinekitClient](https://github.com/qtquickvcp/QtQuickVcp#download).

## Running

To start this configuration either use `mklauncher` or directly start it via the `run.p`.

```bash
./run.py
```

Once started connect via the MachinekitClient.
