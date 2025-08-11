#!/usr/bin/env python3

import argparse
import configparser
import os
import re
import shlex
import socket
import subprocess
import sys
from pathlib import Path
from typing import List, Dict, Tuple

WRAPPER_DIR = Path(__file__).resolve().parent
LIB_DIR = WRAPPER_DIR / 'lib'
LIB_MPI_TRACE = LIB_DIR / 'libmpi_trace.so'


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


def build_sync_envs(groups: List[List[str]], base_env: Dict[str, str], expected_total: int) -> List[Dict[str, str]]:
    envs: List[Dict[str, str]] = []
    if not groups:
        return envs
    coord_host = groups[0][0]
    coord_ip = resolve_ip(coord_host)
    for gi, _ in enumerate(groups):
        e = dict(base_env)
        e['RDMASYNC_ENABLE'] = '1'
        if gi == 0:
            e['RDMASYNC_ROLE'] = 'AUTO'
            e['RDMASYNC_BIND_IP'] = coord_ip
            if expected_total > 0:
                e['RDMASYNC_EXPECTED'] = str(expected_total)
            e['RDMASYNC_SERVER_IP'] = coord_ip
        else:
            e['RDMASYNC_ROLE'] = 'participant'
            e['RDMASYNC_SERVER_IP'] = coord_ip
        envs.append(e)
    return envs


def run_parallel(commands: List[List[str]], envs: List[Dict[str, str]], cwd: Path, verbose: bool) -> int:
    procs = []
    for i, (cmd, env) in enumerate(zip(commands, envs)):
        penv = os.environ.copy()
        penv.update(env)
        dbg(f"Launching [{i}]: {' '.join(cmd)}", verbose)
        # procs.append(subprocess.Popen(cmd, cwd=str(cwd), env=penv))
    rc = 0
    for i, p in enumerate(procs):
        r = p.wait()
        dbg(f"Process [{i}] exited with {r}", verbose)
        rc = rc or r
    return rc


def build_mpirun_commands(base_cmd_tokens: List[str], groups: List[List[str]], export_env: List[Dict[str, str]], verbose: bool) -> List[List[str]]:
    cmds: List[List[str]] = []
    for i, hosts in enumerate(groups):
        print(hosts)
        hosts_csv = ','.join(hosts)
        tokens = substitute_tphosts(base_cmd_tokens, hosts_csv)
        tokens_ext = list(tokens)
        for k, v in export_env[i].items():
            if k.startswith('RDMASYNC_') or k == 'LD_PRELOAD':
                tokens_ext.insert(0, f'{k}={v}')
        cmds.append(tokens_ext)
        dbg(f"Command hosts={hosts_csv}: {' '.join(tokens_ext)}", verbose)
    return cmds


def parse_args(argv: List[str]) -> Tuple[argparse.Namespace, List[str]]:
    p = argparse.ArgumentParser(description='TPBench Wrapper (tpwrapper)', allow_abbrev=False, add_help=False)
    p.add_argument('--help', action='help', help='show this help message and exit')
    p.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    p.add_argument('--nmpi', type=int, default=1, help='Number of mpirun processes')
    p.add_argument('--configfile', type=str, default=None, help='Configuration INI file')
    p.add_argument('--tphosts', type=str, default=None, help='hosts list or pattern (e.g., node[1-8])')
    p.add_argument('--sync', dest='sync', action='store_true', default=True, help='Enable synchronization (default)')
    p.add_argument('--trace', action='store_true', help='Enable tracing (LD_PRELOAD libmpi_trace.so)')
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
        envs = build_sync_envs(groups, base_env, total_ranks) if sync_enabled else [base_env.copy() for _ in groups]

        commands = build_mpirun_commands(base_tokens, groups, envs, verbose)
        return run_parallel(commands, envs, WRAPPER_DIR, verbose)

    if not rest:
        print('Usage: tpwrapper.py [options] <your_program> [your_program options]', file=sys.stderr)
        return 2

    sync_enabled = args.sync
    trace_enabled = args.trace

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

    if groups and (base_tokens[0].endswith('mpirun') or base_tokens[0].endswith('mpiexec')):
        envs = build_sync_envs(groups, base_env, total_ranks) if sync_enabled else [base_env.copy() for _ in groups]
        commands = build_mpirun_commands(base_tokens, groups, envs, verbose)
        return run_parallel(commands, envs, WRAPPER_DIR, verbose)
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

