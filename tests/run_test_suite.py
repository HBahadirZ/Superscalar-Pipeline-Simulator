#!/usr/bin/env python3
import math
import re
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJ = ROOT / "proj"
GEN_DIR = ROOT / "tests" / "generated_traces"


def _to_wsl_path(path: Path) -> str:
    drive = path.drive.rstrip(":").lower()
    suffix = path.as_posix().split(":", 1)[1]
    return f"/mnt/{drive}{suffix}"


def _run_raw(args):
    # On Windows host, execute Linux ELF through WSL.
    if sys.platform.startswith("win"):
        if args and args[0] == str(PROJ):
            wsl_cwd = _to_wsl_path(ROOT)
            wsl_args = []
            for arg in args:
                p = Path(arg)
                if p.exists():
                    wsl_args.append(_to_wsl_path(p))
                else:
                    wsl_args.append(arg)
            quoted = " ".join(f'"{a}"' if " " in a else a for a in wsl_args)
            cmd = f'cd "{wsl_cwd}" && {quoted}'
            return subprocess.run(["wsl", "bash", "-lc", cmd], capture_output=True, text=True)
    return subprocess.run(args, capture_output=True, text=True, cwd=ROOT)


def run_proj(trace_path: Path, start: int, count: int, d_value: int):
    cmd = [str(PROJ), str(trace_path), str(start), str(count), str(d_value)]
    return _run_raw(cmd)


def parse_metrics(stdout: str):
    out = {}
    patterns = {
        "cycles": r"Total Cycles:\s*([0-9]+)",
        "exec_ms": r"Execution Time \(in ms\):\s*([0-9eE+\-\.]+)",
        "int": r"%Int:\s*([0-9eE+\-\.]+)%",
        "fp": r"%FP:\s*([0-9eE+\-\.]+)%",
        "branch": r"%Branch:\s*([0-9eE+\-\.]+)%",
        "load": r"%Load:\s*([0-9eE+\-\.]+)%",
        "store": r"%Store:\s*([0-9eE+\-\.]+)%",
    }
    for key, pat in patterns.items():
        m = re.search(pat, stdout)
        if m:
            out[key] = float(m.group(1))
    return out


def write_trace(name: str, lines):
    path = GEN_DIR / name
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


class PipelineSuite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        GEN_DIR.mkdir(parents=True, exist_ok=True)
        if not PROJ.exists():
            raise FileNotFoundError(
                "Executable 'proj' not found. Run `make` before running tests."
            )

        cls.trace_single_int = write_trace(
            "single_int.trace",
            [
                "1000,1",
            ],
        )

        cls.trace_three_int = write_trace(
            "three_int.trace",
            [
                "1000,1",
                "1004,1",
                "1008,1",
            ],
        )

        cls.trace_fp_chain = write_trace(
            "fp_chain.trace",
            [
                "1000,2",
                "1004,1,1000",
                "1008,2,1004",
                "100C,1,1008",
            ],
        )

        cls.trace_load_chain = write_trace(
            "load_chain.trace",
            [
                "2000,4",
                "2004,1,2000",
                "2008,4,2004",
                "200C,1,2008",
            ],
        )

        cls.trace_branch_block = write_trace(
            "branch_block.trace",
            [
                "3000,3",
                "3004,1",
                "3008,1",
                "300C,1",
            ],
        )

        cls.trace_no_branch = write_trace(
            "no_branch.trace",
            [
                "4000,1",
                "4004,1",
                "4008,1",
                "400C,1",
            ],
        )

        cls.trace_mixed = write_trace(
            "mixed.trace",
            [
                "5000,1",
                "5004,2,5000",
                "5008,3,5004",
                "500C,4,5000",
                "5010,5,500C",
            ],
        )

    def test_happy_path_runs_and_reports_metrics(self):
        result = run_proj(self.trace_mixed, 1, 5, 2)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        metrics = parse_metrics(result.stdout)
        self.assertIn("cycles", metrics)
        self.assertGreater(metrics["cycles"], 0)
        total_hist = (
            metrics["int"] + metrics["fp"] + metrics["branch"] + metrics["load"] + metrics["store"]
        )
        self.assertAlmostEqual(total_hist, 100.0, delta=0.05)

    def test_zero_instruction_count_is_zero_stats(self):
        result = run_proj(self.trace_single_int, 1, 0, 1)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        metrics = parse_metrics(result.stdout)
        self.assertEqual(metrics.get("cycles"), 0.0)
        self.assertEqual(metrics.get("int"), 0.0)
        self.assertEqual(metrics.get("fp"), 0.0)
        self.assertEqual(metrics.get("branch"), 0.0)
        self.assertEqual(metrics.get("load"), 0.0)
        self.assertEqual(metrics.get("store"), 0.0)

    def test_wrong_arity_rejected(self):
        result = _run_raw([str(PROJ)])
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_d_rejected(self):
        result = run_proj(self.trace_single_int, 1, 1, 99)
        self.assertNotEqual(result.returncode, 0)

    def test_missing_trace_should_fail(self):
        missing = GEN_DIR / "does_not_exist.trace"
        result = run_proj(missing, 1, 1, 1)
        self.assertNotEqual(
            result.returncode,
            0,
            msg="Missing trace should return non-zero exit code.",
        )

    def test_start_instruction_less_than_one_should_fail(self):
        result = run_proj(self.trace_single_int, 0, 1, 1)
        self.assertNotEqual(
            result.returncode,
            0,
            msg="start_inst < 1 should be rejected by argument validation.",
        )

    def test_requesting_more_instructions_than_trace_has_should_fail(self):
        result = run_proj(self.trace_three_int, 1, 10, 1)
        self.assertNotEqual(
            result.returncode,
            0,
            msg="Trace exhaustion should be treated as invalid run input.",
        )

    def test_execution_time_matches_frequency_formula(self):
        freqs = {1: 1.0, 2: 1.2, 3: 1.7, 4: 1.8}
        for d in (1, 2, 3, 4):
            with self.subTest(d=d):
                result = run_proj(self.trace_three_int, 1, 3, d)
                self.assertEqual(result.returncode, 0, msg=result.stderr)
                metrics = parse_metrics(result.stdout)
                expected = metrics["cycles"] / (freqs[d] * 1e6)
                # Output uses default C++ stream formatting, so allow small rounding error.
                self.assertTrue(math.isclose(metrics["exec_ms"], expected, rel_tol=1e-5, abs_tol=1e-10))

    def test_d2_fp_latency_increases_cycles(self):
        d1 = run_proj(self.trace_fp_chain, 1, 4, 1)
        d2 = run_proj(self.trace_fp_chain, 1, 4, 2)
        self.assertEqual(d1.returncode, 0, msg=d1.stderr)
        self.assertEqual(d2.returncode, 0, msg=d2.stderr)
        c1 = parse_metrics(d1.stdout)["cycles"]
        c2 = parse_metrics(d2.stdout)["cycles"]
        self.assertGreater(c2, c1)

    def test_d3_load_latency_increases_cycles(self):
        d1 = run_proj(self.trace_load_chain, 1, 4, 1)
        d3 = run_proj(self.trace_load_chain, 1, 4, 3)
        self.assertEqual(d1.returncode, 0, msg=d1.stderr)
        self.assertEqual(d3.returncode, 0, msg=d3.stderr)
        c1 = parse_metrics(d1.stdout)["cycles"]
        c3 = parse_metrics(d3.stdout)["cycles"]
        self.assertGreater(c3, c1)

    def test_branch_fetch_stall_costs_cycles(self):
        no_branch = run_proj(self.trace_no_branch, 1, 4, 1)
        with_branch = run_proj(self.trace_branch_block, 1, 4, 1)
        self.assertEqual(no_branch.returncode, 0, msg=no_branch.stderr)
        self.assertEqual(with_branch.returncode, 0, msg=with_branch.stderr)
        c_no = parse_metrics(no_branch.stdout)["cycles"]
        c_br = parse_metrics(with_branch.stdout)["cycles"]
        self.assertGreater(c_br, c_no)


if __name__ == "__main__":
    unittest.main(verbosity=2)
