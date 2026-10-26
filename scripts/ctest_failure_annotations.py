#!/usr/bin/env python3
"""
Print failed ctest cases from a JUnit file (ctest --output-junit) as GitHub ::error:: annotations,
so the failing test names and their first assertion lines are readable on the run page without
downloading the logs. Usage: ctest_failure_annotations.py ctest_results.xml
"""
import sys
import xml.etree.ElementTree as ET


def clean(text, limit=900):
    text = " | ".join(line.strip() for line in (text or "").splitlines() if line.strip())
    # Workflow commands end at a newline and treat % specially.
    return text.replace("%", "%25")[:limit]


def main(path):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        print("::warning::no readable ctest results (%s)" % exc)
        return 0
    failed = 0
    for case in root.iter("testcase"):
        problem = case.find("failure")
        if problem is None:
            problem = case.find("error")
        if problem is None:
            continue
        failed += 1
        # Catch2 prints the failing assertion with "FAILED:" and the expansion just after; keep that part of the output.
        output = (case.findtext("system-out") or "") + "\n" + (problem.get("message") or "") + "\n" + (problem.text or "")
        lines = output.splitlines()
        start = next((i for i, l in enumerate(lines) if "FAILED" in l or "error" in l.lower()), 0)
        detail = clean("\n".join(lines[max(0, start - 3):start + 12]))
        print("::error title=Test failed: %s::%s" % (case.get("name", "?").replace(":", " ").replace(",", " "), detail or "(no output)"))
    print("%d failed test(s)" % failed)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "ctest_results.xml"))
