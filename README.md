# fpga_perfect_hash

_Note_: This is currently a work in progress - though it is sufficient
for my current needs, it is not fully tested or evaluated. My plan
is to clean it up and write a paper at some point, but that may
never get done. Any issues raised are likely to be ignored or
closed, and pull requests are not currently solicited.

This is a set of tools designed to create and implement perfect hash functions
for FPGAs. The intended use case is for relatively small key sets (say up
to 1000s of keys), and moderately wide inputs (up to around 32 bits).

The hash function itself consists of k independent 1-bit wide
by 2^a entry ROMs. The k 1-bit outputs give the hash, supporting up to 2^k keys.
The a address bits of each ROM are connected randomly to the input key bits.
The problem then comes in finding a configuration of the k*2^a address
bits, so that the keys do not conflict. This is solved by phrasing
the problem as a boolean satisfiability problem.

This project relies on [minisat](http://minisat.se/) by
Niklas Een and Niklas Sorensson for the bsat side of things; a copy
is embedded in this project,as minisat is used as a library. The
license file for this project does not apply to minisat, which has
its own license.

## License

fpga_perfect_hash - a tool for creating LUT-based perfect hashes.
Copyright (C) 2016  David B. Thomas

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
