# ZeroLedge

This project is a reference implementation of ZeroLedge, a zero-knowledge proof-of-liabilities system for use by banks and
bitcoin exchanges. It provides a mechanism for proving in a distributed fashion that the liability incurred upon a financial
institution by individual depositors is less than or equal to a published quantity, while revealing no information to any
party except for that quantity. For complete information about the algorithm, consult the paper

ZeroLedge: Proving Solvency with Privacy (2016)  
Jack Doerner, David Evans, and abhi shelat

While this project is intended to demonstrate the usefulness and practicality of the ZeroLedge system, it is not guaranteed
to be free of defects or suitable for use in production environments.

## Installation

ZeroLedge requires the source for the MIRACL big number library to be located in the `miracl` subdirectory. It should not be
compiled; the build system for ZeroLedge will automatically compile MIRACL with the appropriate options on x86 and x86_64.
The latest version of MIRACL may be downloaded from <https://www.certivox.com/miracl>.

Once the MIRACL source is correctly located, the software can be built simply by calling `make` from within the project root.

## Usage

This software comprises two distinct programs, `zlgenerate` and `zlverify`, for use by provers (generally, financial
institutions and asset holders) and verifiers (depositors into said instititutions, and third parties) respectively. The
interaction between these two programs and their users proceeds thus:

1. The prover uses `zlgenerate` to ingest a private ledger and produce a proof transcript and a list of proof openers.
2. The prover publishes the proof transcript on a public channel, where it cannot be modified or retracted.
3. The prover transmits privately to each account holder the proof opener associated with their account.
4. Each account holder uses `zlverify` and the proof opener provided by the prover to verify the inclusion and correctness
of their own account in the proof transcript.
5. Account holders and third parties use `zlverify` to verify the integrity of the proof transcript.

### Ledger Formatting

The ledger file ingested by the proof generator is a plain text file of the following format. The first line contains an
integer representing the public upper bound on liabilities (in most case, this will be the assets). Each subsequent line
represents a single ledger entry and contains two fields, delimited by a space. First, a string representing the account
identifier associated with the ledger entry, and second, an integer representing the balance associated with the account.

### Proof Generation

The `zlgenerate` program is used to generate a proof transcript from a ledger document. The basic format for the command is
`zlgenerate -e <entries_output> -o <proof_output> <ledger_input>`. When called with no input or output options, `zlgenerate`
will read its ledger from `stdin` and write its proof to `stdout`, sending status messages to `stderr`. By default, it spawns
a number of threads equal to the number of detected processors, but the thread count can manually be controlled with the `-t`
flag. Additional flags are available for controlling advanced parameters; more information can found using the `-h` flag.

### Proof Verification

The `zlverify` program is used to verify the integrity of a proof transcript, and optionally verify the inclusion of one or
more known ledger entries in the proof. The basic format for the command is `zlverify <proof_input>`, and inclusion of known
entries can be verified with `zlverify -k <entries_input> <proof_input>`, where `<entries_input>` is a text file formatted
identically to the `<entries_output>` produced by `zlgenerate`. The `-i` flag can be used to verify inclusion only, and omit
integrity verification. By default, `zlverify` spawns a number of threads equal to the number of detected processors, but the
thread count can manually be controlled with the `-t` flag. Additional flags are available for controlling advanced
parameters; more information can found using the `-h` flag.

## Source Layout

The sources for the  proof generator and verifier may be found respectively in `zlgenerate.cpp` and `zlverify.cpp`. The code
in these files mainly deals with IO, setup, threading, options, etc. The various components of the ZeroLedge algorithm
itself are implemented by four main source files:

* `ledger.h` contains the basic data structures which represent ledgers, ledger entries, and proofs. Some of these data
structures also contain methods for relating and managing their various internal parts.
* `lepprocessor.h` contains the code responsible for computing ledger entry commitments and proofs.
* `lbpprocessor.h` contains the code responsible for computing ledger bit commitments and proofs.
* `dbpprocessor.h` contains the code responsible for computing difference bit commitments and proofs.

All of the files mentioned above contain comments explaining their overall layout, along with references to relevant details in the paper.

## Credits

Author: Jack Doerner

## License

This software is provided under the GNU General Public License, Version 3. See LICENSE for details.