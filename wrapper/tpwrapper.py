#!/usr/bin/env python3

import argparse
import configparser
import os
import re
import shlex
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Dict, Tuple

WRAPPER_DIR = Path(__file__).resolve().parent
LIB_DIR = WRAPPER_DIR / 'lib'
LIB_MPI_TRACE = LIB_DIR / 'libmpi_trace.so'

TPWRAPPER_TIME_STAMP=time.strftime("%Y%m%d_%H%M%S", time.localtime()) + f"_{int(time.time() * 1000000) % 1000000:06d}"

def dbg(msg: str, verbose: bool):
    if verbose:
        print(f"[tpwrapper] {msg}")


def expand_hosts(expr: str) -> List[str]:
    expr = expr.strip()
    if not expr:
        return []
    parts = []
    for token in expr.split(','):
        token = token.strip()
        m = re.match(r'^(?P<prefix>[^\[]*)(?:\[(?P<start>\d+)-(?:\s*)?(?P<end>\d+)\])?(?P<suffix>.*)$', token)
        if m and m.group('start') and m.group('end'):
            start = m.group('start'); end = m.group('end')
            width = max(len(start), len(end))
            a, b = int(start), int(end)
            rng = range(a, b + 1) if a <= b else range(a, b - 1, -1)
            for i in rng:
                parts.append(f"{m.group('prefix')}{i:0{width}d}{m.group('suffix')}")
        else:
            parts.append(token)
    return parts


def chunk_hosts(hosts: List[str], nmpi: int) -> List[List[str]]:
    nmpi = max(1, nmpi)
    host_per_node = max(1, len(hosts) // nmpi)
    if nmpi >= len(hosts):
        return [[h] for h in hosts] + [[] for _ in range(nmpi - len(hosts))]
    out = [[] for _ in range(nmpi)]
    for i, h in enumerate(hosts):
        out[i // host_per_node].append(h)
    return out


def resolve_ip(host: str) -> str:
    try:
        return socket.gethostbyname(host)
    except Exception:
        return host


def parse_ppn_ranks_from_cmd(cmd_tokens: List[str]) -> int:
    ppn = 0
    i = 0
    while i < len(cmd_tokens):
        if cmd_tokens[i] in ('-ppn') and i + 1 < len(cmd_tokens):
            try:
                ppn = int(cmd_tokens[i + 1])
            except ValueError:
                pass
            i += 2
        else:
            i += 1
    return ppn


def substitute_tphosts(cmd_tokens: List[str], hosts_csv: str) -> List[str]:
    return [hosts_csv if tok == 'TPHOSTS' else tok for tok in cmd_tokens]


def ensure_trace_env(env: Dict[str, str], enable_trace: bool):
    if enable_trace:
        if not LIB_MPI_TRACE.exists():
            raise FileNotFoundError(f"Tracing library not found: {LIB_MPI_TRACE}")
        prev = env.get('LD_PRELOAD', '')
        lib = str(LIB_MPI_TRACE)
        env['LD_PRELOAD'] = f"{lib} {prev}".strip()


def build_sync_envs(groups: List[List[str]], base_env: Dict[str, str], expected_total: int, verbose: bool) -> List[Dict[str, str]]:
    envs: List[Dict[str, str]] = []
    if not groups:
        return envs
    coord_host = groups[0][0]
    coord_ip = resolve_ip(coord_host)
    
    # Calculate expected connections for coordinator
    # Coordinator expects connections from all other groups
    participant_groups = len(groups) - 1
    
    for gi, _ in enumerate(groups):
        e = dict(base_env)
        e['RDMASYNC_ENABLE'] = '1'
        if (verbose):
            e['RDMASYNC_DEBUG'] = '1'
        if gi == 0:
            e['RDMASYNC_ROLE'] = 'coordinator'
            e['RDMASYNC_BIND_IP'] = coord_ip
            # Coordinator expects one connection from each participant group
            if participant_groups > 0:
                e['RDMASYNC_EXPECTED'] = str(participant_groups)
            e['RDMASYNC_SERVER_IP'] = coord_ip
        else:
            e['RDMASYNC_ROLE'] = 'participant'
            e['RDMASYNC_SERVER_IP'] = coord_ip
        envs.append(e)
    return envs


def run_parallel(commands: List[List[str]], envs: List[Dict[str, str]], cwd: Path, log_storage_prefix: str = None, verbose: bool = False) -> int:
    def get_hosts(cmd: List[str]) -> str:
        # Extract hosts from the command line arguments
        for i, token in enumerate(cmd):
            if token in ('-hosts', '--hosts') and i + 1 < len(cmd):
                return cmd[i+1]
        return ""

    procs = []
    fds = []
    os.makedirs(log_storage_prefix, exist_ok=True)
    for i, (cmd, env) in enumerate(zip(commands, envs)):
        node_list = get_hosts(cmd)
        cmd_output_path = os.path.join(log_storage_prefix, f"{node_list}.out") if node_list else None
        cmd_err_path = os.path.join(log_storage_prefix, f"{node_list}.err") if node_list else None
        penv = os.environ.copy()
        penv.update(env)
        out_f = open(cmd_output_path, 'w') if cmd_output_path else None
        err_f = open(cmd_err_path, 'w') if cmd_err_path else None
        dbg(f"Launching [{i}]: {' '.join(cmd)}", True)
        p = subprocess.Popen(cmd, cwd=str(cwd), env=penv, stdout=out_f, stderr=err_f)
        procs.append(p)
        fds.append((out_f, err_f))

    rc = 0
    for i, p in enumerate(procs):
        r = p.wait()
        out_f, err_f = fds[i]
        if out_f:
            out_f.close()
        if err_f:
            err_f.close()
        dbg(f"Process [{i}] exited with {r}", True)
        rc = rc or r
    return rc

def detect_mpi_impl(mpirun_exe: str) -> str:
    """
    Detect MPI implementation for the given mpirun/mpiexec executable.
    Returns one of: 'openmpi', 'oneapi', 'mpich', or 'unknown'.
    Strategy:
      1) Try `mpirun -V` or `mpirun --version` and parse output.
      2) Fall back to environment variables.
    """
    # Try version flags first (they should return quickly and not launch jobs)
    for flag in ('-V', '--version'):
        try:
            out = subprocess.check_output([mpirun_exe, flag], stderr=subprocess.STDOUT, timeout=2)
            txt = out.decode(errors='ignore').lower()
            if 'open mpi' in txt or 'ompi' in txt:
                return 'openmpi'
            if 'intel(r) mpi' in txt or 'oneapi' in txt or 'intel mpi' in txt:
                return 'oneapi'
            if 'mpich' in txt or 'hydra' in txt:
                return 'mpich'
        except Exception:
            pass

    # Fall back to well-known environment hints
    env = os.environ
    if any(k in env for k in ('I_MPI_ROOT', 'I_MPI_SUBSTITUTE_HOSTS', 'FI_PROVIDER_PATH')):
        return 'oneapi'
    if any(k in env for k in ('OMPI_VERSION', 'OMPI_COMM_WORLD_SIZE', 'OMPI_MCA_orte_base_help_aggregate')):
        return 'openmpi'
    if any(k in env for k in ('MPICH_VERSION', 'HYDRA_VERSION', 'PMI_RANK')):
        return 'mpich'

    return 'unknown'


def build_mpirun_commands(base_cmd_tokens: List[str], groups: List[List[str]], export_env: List[Dict[str, str]], trace_storage_prefix: str, verbose: bool) -> List[List[str]]:
    # Detect implementation from the first segment's launcher (assume same mpirun across segments)
    mpirun_exe = base_cmd_tokens[0] if base_cmd_tokens else 'mpirun'
    mpi_impl = detect_mpi_impl(mpirun_exe)

    cmds: List[List[str]] = []
    for i, hosts in enumerate(groups):
        dbg(f"Hosts group {i}: {hosts}", verbose)
        hosts_csv = ','.join(hosts)
        tokens = substitute_tphosts(base_cmd_tokens, hosts_csv)
        tokens_ext = list(tokens)

        # Determine where to insert flags: right after 'mpirun/mpiexec'
        insert_idx = 1 if tokens_ext and (tokens_ext[0].endswith('mpirun') or tokens_ext[0].endswith('mpiexec')) else 0

        # Build implementation-specific env propagation flags
        env_items = [(k, str(v)) for k, v in export_env[i].items() if (k.startswith('RDMASYNC_') or k == 'LD_PRELOAD' or k.startswith('OMP'))]
        if trace_storage_prefix is not None and len(trace_storage_prefix) > 0:
            env_items.append(('MPI_LOG_PATH_PREFIX', trace_storage_prefix))
        env_flags: List[str] = []
        if mpi_impl == 'openmpi':
            # Use: -x VAR=VALUE
            for k, v in env_items:
                env_flags += ['-x', f'{k}={v}']
        elif mpi_impl in ('oneapi', 'mpich'):
            # Hydra: -genv VAR VALUE
            for k, v in env_items:
                env_flags += ['-genv', k, v]
        else:
            # Fallback to Open MPI style if unknown
            for k, v in env_items:
                env_flags += ['-x', f'{k}={v}']

        # Insert flags right after the launcher token
        tokens_ext[insert_idx:insert_idx] = env_flags

        cmds.append(tokens_ext)
        dbg(f"Command hosts={hosts_csv} [{mpi_impl}]: {' '.join(tokens_ext)}", verbose)

    return cmds


def parse_args(argv: List[str]) -> Tuple[argparse.Namespace, List[str]]:
    p = argparse.ArgumentParser(description='TPBench Wrapper (tpwrapper)', allow_abbrev=False, add_help=False)
    p.add_argument('--help', action='help', help='show this help message and exit')
    p.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    p.add_argument('--configfile', type=str, default=None, help='Configuration INI file')
    p.add_argument('--tphosts', type=str, default=None, help='hosts list or pattern (e.g., node[1-8])')
    p.add_argument('--sync', dest='sync', action='store_true', default=True, help='Enable synchronization (default)')
    p.add_argument('--trace', action='store_true', help='Enable tracing (LD_PRELOAD libmpi_trace.so)')
    p.add_argument('--no-sync', dest='sync', action='store_false', help='Disable synchronization')

    p.add_argument('--nmpi', type=int, default=1, help='Number of mpirun processes')
    p.add_argument('--n_per_mpirun', type=int, default=None, action='store', help='number of processes per mpirun process (deprecated, use --nmpi)')
    p.add_argument('--ppn', type=int, default=None, action='store', help='number of processes per node')

    p.add_argument('--merge_trace_files', action='store_true', help='Let trace_storage_prefix=tpmpi_trace_logs/$time_stamp, merge all mpirun processes\' trace files into a single trace file')
    p.add_argument('--trace_storage_prefix', type=str, default=None, help='Prefix for trace storage (default: None), trace files will be stored in trace_storage_prefix/$hostname/mpi_trace_rank_$rankid.log')
    p.add_argument('--log_storage_prefix', type=str, default=f"./tpwrapper_logs/{TPWRAPPER_TIME_STAMP}", help='Prefix for mpirun log storage (default: ./tpwrapper_logs/$time_stamp), log files will be stored in log_storage_prefix/$hosts.log, hosts is the hosts list running an mpirun process.')

    args, rest = p.parse_known_args(argv)
    return args, rest


def load_config(path: Path):
    cfg = configparser.ConfigParser()
    with path.open('r') as f:
        cfg.read_file(f)
    tp = cfg['tpwrapper'] if 'tpwrapper' in cfg else {}
    prog = cfg['program'] if 'program' in cfg else {}
    hosts_sec = cfg['tphosts'] if 'tphosts' in cfg else {}
    return cfg, tp, prog, hosts_sec


def main(argv: List[str]) -> int:
    args, rest = parse_args(argv)
    verbose = args.verbose

    if args.configfile:
        cfg_path = Path(args.configfile).resolve()
        cfg, tp, prog, hosts_sec = load_config(cfg_path)

        sync_enabled = cfg.getboolean('tpwrapper', 'sync', fallback=True)
        trace_enabled = cfg.getboolean('tpwrapper', 'trace', fallback=False)
        nmpi = cfg.getint('tpwrapper', 'nmpi', fallback=args.nmpi)
        hosts_expr = cfg.get('tpwrapper', 'tphosts', fallback='')

        mpi_opts = cfg.get('program', 'mpi_options', fallback='')
        mpi_prog = cfg.get('program', 'mpi_program', fallback='')
        if not mpi_prog:
            print('Error: [program].mpi_program is required in configfile', file=sys.stderr)
            return 2

        groups: List[List[str]] = []
        if hosts_sec:
            for k in sorted(hosts_sec.keys()):
                if not k.startswith('mpi_'):
                    continue
                groups.append([h.strip() for h in hosts_sec.get(k).split(',') if h.strip()])
        else:
            all_hosts = expand_hosts(hosts_expr)
            if not all_hosts and args.tphosts:
                all_hosts = expand_hosts(args.tphosts)
            groups = chunk_hosts(all_hosts, nmpi)

        if not groups:
            print('Error: No tphosts specified (use --tphosts or configfile [tphosts]/tphosts)', file=sys.stderr)
            return 2

        base_cmd = f"mpirun {mpi_opts} {mpi_prog}" if mpi_opts else mpi_prog
        base_tokens = shlex.split(base_cmd)

        ppn = parse_ppn_ranks_from_cmd(base_tokens)
        total_ranks = ppn * len(all_hosts)

        if sync_enabled and total_ranks <= 0:
            print('Warning: Could not parse total ranks (-n). RDMASYNC_EXPECTED will not be set.', file=sys.stderr)

        base_env: Dict[str, str] = {}
        ensure_trace_env(base_env, trace_enabled)
        envs = build_sync_envs(groups, base_env, 0, verbose) if sync_enabled else [base_env.copy() for _ in groups]

        commands = build_mpirun_commands(base_tokens, groups, envs, None, verbose)
        return run_parallel(commands, envs, WRAPPER_DIR, verbose)

    if not rest:
        print('Usage: tpwrapper.py [options] <your_program> [your_program options]', file=sys.stderr)
        return 2

    sync_enabled = args.sync
    trace_enabled = args.trace

    if args.nmpi <= 1:
        sync_enabled = False
        
    trace_storage_prefix = args.trace_storage_prefix
    if args.merge_trace_files == True and trace_storage_prefix is None:
        trace_storage_prefix = f"./tpmpi_trace_logs/{TPWRAPPER_TIME_STAMP}"

    if args.log_storage_prefix is not None:
        log_storage_prefix = args.log_storage_prefix
    else:
        log_storage_prefix = f"./tpwrapper_logs/{TPWRAPPER_TIME_STAMP}"

    groups: List[List[str]] = [[]]
    if args.tphosts:
        all_hosts = expand_hosts(args.tphosts)
        groups = chunk_hosts(all_hosts, max(1, args.nmpi))
    dbg(f"Using tphosts: {groups}", verbose)
    base_tokens = rest

    ppn = parse_ppn_ranks_from_cmd(base_tokens)
    total_ranks = ppn * len(all_hosts)

    base_env: Dict[str, str] = {}
    ensure_trace_env(base_env, trace_enabled)

    if groups and base_tokens[0] in ('mpirun', 'mpiexec'):
        envs = build_sync_envs(groups, base_env, 0, verbose) if sync_enabled else [base_env.copy() for _ in groups]
        commands = build_mpirun_commands(base_tokens, groups, envs, trace_storage_prefix, verbose)
        return run_parallel(commands, envs, WRAPPER_DIR, log_storage_prefix, verbose)
    else:
        hosts_csv = ','.join(groups[0]) if groups and groups[0] else ''
        tokens = substitute_tphosts(base_tokens, hosts_csv)
        penv = os.environ.copy()
        penv.update(base_env)
        dbg(f"Launching: {' '.join(tokens)}", verbose)
        p = subprocess.Popen(tokens, cwd=str(WRAPPER_DIR), env=penv)
        return p.wait()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

