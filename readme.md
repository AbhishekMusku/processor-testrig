# Dynamic x86/x86-64 Instruction Generator for Post-Silicon Processor Validation

This project implements a sophisticated **Random Instruction Testing (RIT) framework for post-silicon processor validation**, a critical methodology used by Intel, AMD, and other major processor manufacturers to discover corner cases and validate processor correctness. The implementation demonstrates advanced understanding of x86-64 instruction encoding, multi-threaded testing, and industry-standard validation techniques that are essential for ensuring processor reliability before market release.

## Technical Architecture and Implementation

The project's three-part progression demonstrates increasing sophistication in processor validation methodologies:

**Part 1: Fundamental Instruction Encoding**
The implementation of MOV instruction variants (register-to-register, immediate-to-register, register-to-memory, memory-to-register) requires deep understanding of x86-64 instruction encoding complexity. The **variable-length instruction format** with REX prefixes, ModR/M bytes, and SIB bytes represents one of the most challenging aspects of modern processor validation. Supporting both 32-bit and 64-bit modes with proper REX prefix handling demonstrates mastery of instruction set architecture intricacies.

**Part 2: Execution Framework Development**
The transition from static instruction generation to dynamic execution using `mmap()` with executable permissions showcases advanced systems programming skills. Creating proper function prologues and epilogues while preserving callee-saved registers demonstrates understanding of Application Binary Interface (ABI) compliance—a critical requirement for processor validation that ensures generated test sequences don't corrupt system state.

**Part 3: Multi-Processing and Concurrency Testing**
The implementation of multi-threaded and multi-process execution using `fork()` and CPU affinity binding via `sched_setaffinity()` mirrors **industry-standard multi-core validation techniques**. This approach enables testing of cache coherence, memory ordering, and inter-processor communication—areas where many subtle processor bugs manifest.

## Setup and Usage Instructions

### Prerequisites
- **Linux x86-64 system** (required for CPU affinity and executable memory mapping)
- **GCC compiler** with support for inline assembly and system headers
- **GNU Make** for building the project
- **GDB** (optional, for debugging generated instructions)
- **Root privileges may be required** for CPU affinity binding in some configurations

### Building the Project

```bash
# Clone the repository
git clone https://github.com/AbhishekMusku/processor-testrig.git
cd processor-testrig

# Build the executable
make

# Clean build files (optional)
make clean
```

### Running the Project

The basic command structure:
```bash
./encodeit [seed] [num_instructions] [num_threads] [logfile]
```

**Parameters:**
- `seed` (optional): Random seed for reproducible test generation (default: 12345)
- `num_instructions` (optional): Number of instructions to generate per thread (default: 10)
- `num_threads` (optional): Number of concurrent processes/threads (default: 1, max: 4)
- `logfile` (optional): Output log file for detailed instruction logging

### Example Usage

```bash
# Basic single-threaded execution
./encodeit

# Full logging with custom parameters
./encodeit 9999 100 2 validation.log
```

The generated test programs validate processor functionality through:
- **Instruction encoding correctness**: Ensures proper x86-64 opcode generation
- **Memory consistency**: Tests load/store ordering with barriers
- **Multi-core coherence**: Validates cache coherency protocols
- **ABI compliance**: Preserves callee-saved registers and stack frames
