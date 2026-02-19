# TPBench - Traverse between Profiling and Benchmarking

TPBench is a benchmark tool for computing systems. It integrates facilities for measuring runtime and events, creating benchmark workloads, creating benchmark score rules, and analyzing performance data.

With TPBench, it's easy to traverse between profiling and benchmarking, and to record and track performance:
- `tpbcli run`: Evaluate the performance and health with common micro computing kernels (e.g. arithmetic instructions, the STREAM benchmark, stencils, AI operators, etc).
- `tpbcli benchmark`: Score and compare the performance from multiple dimensions you care.
- `tpbcli profile`: Profile a part of your codes and turn it into a benchmark target.
- `tpbcli record`: Record and track the performance.

Refer to documentations in `docs/` for:
- Build and basic usages ([docs/USAGE.md](docs/USAGE.md)). 基础编译及使用方法：[USAGE_CN.md](docs/USAGE_CN.md).
- Programming interfaces ([docs/API_Reference.md](docs/API_Reference.md)).
- Case studies and examples ([docs/EXAMPLES.md](docs/EXAMPLES.md)).

## License

TPBench

Copyright (C) 2024 - 2026 Key Liao (Liao Qiucheng), Center for High-Performance Computing, Shanghai Jiao Tong University.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

