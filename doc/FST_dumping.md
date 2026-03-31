# FST Waveform Dumping in Bluesim

FST dumping works exactly like VCD — just use `-F` instead of `-V`.

## Command Line

```sh
# FST with explicit filename
./mkMyDesign.bexe -F waves.fst

# FST with default filename (dump.fst)
./mkMyDesign.bexe -F

# VCD still works as before
./mkMyDesign.bexe -V waves.vcd
```

## Memory / Array Dumping

By default, `mkRegFile` and BRAM contents are **not** included in waveform
dumps.  The `-A` flag opts in to memory dumping (FST only).

### Flat mode (default, Surfer / GTKWave compatible)

Each memory element becomes a flat signal named `<inst>[<index>]` in the
parent scope, matching the convention used by Verilog simulators.

```sh
# Dump all memories (no depth limit)
./mkMyDesign.bexe -F waves.fst -A

# Dump only memories with <= 256 entries
./mkMyDesign.bexe -F waves.fst -A 256
```

### Array-scope mode

Uses the FST `SV_ARRAY` scope type and `ARRAY` attribute.  This preserves
richer structural information but may not be supported by all viewers.

```sh
./mkMyDesign.bexe -F waves.fst -As
```

### Interactive Tcl Commands

```sh
# Enable flat memory dumping (before starting FST)
sim fst arrays on

# Enable with a depth limit
sim fst arrays 256

# Enable with FST SV_ARRAY scope
sim fst arrays scope

# Disable memory dumping
sim fst arrays off
```

> **Note:** Memory tracing must be enabled *before* `sim fst on` (or `-F`)
> opens the FST file, since signal definitions are written at that point.

## Interactive Tcl Commands

Via `-c` or `-f`:

```sh
# Enable FST with a filename
./mkMyDesign.bexe -c 'sim fst waves.fst; sim step 1000'

# Toggle dumping on/off
./mkMyDesign.bexe -c 'sim fst waves.fst; sim step 500; sim fst off; sim step 500; sim fst on; sim step 500'
```

## Setup

Make sure your `PATH` includes the local BSC install so the `.bexe` script
picks up the updated `bluetcl` and `bluesim.tcl`:

```sh
export PATH=/path/to/bsc/inst/bin:$PATH
```

## Viewing FST Files

FST files can be viewed in [Surfer](https://surfer-project.org/),
[GTKWave](http://gtkwave.sourceforge.net/), or converted to VCD with:

```sh
fst2vcd waves.fst > waves.vcd
```

The `fstminer` utility (from GTKWave) is useful for inspecting signal
hierarchies without a GUI:

```sh
fstminer waves.fst | head -20
```
