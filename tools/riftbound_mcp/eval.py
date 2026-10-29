"""Measure how well a local model picks tools and arguments.

Usage:
    python3 eval.py --model riftbound
    python3 eval.py --model qwen3.5:9b --model riftbound   # compare

Each case defines the question, the expected tool (a name, or a list of
acceptable names) and the arguments that must match (string arguments match
as case-insensitive substrings). Add your
own cases to eval_cases.json to measure every change to the prompt, the
Modelfile or the model.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time
from pathlib import Path

import ollama
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

from chat import SYSTEM, mcp_to_ollama

HERE = Path(__file__).resolve().parent


def matches(expected: dict, got: dict) -> bool:
    for k, v in expected.items():
        g = got.get(k)
        if isinstance(v, str) and isinstance(g, str):
            if v.lower() not in g.lower():
                return False
        elif g != v:
            return False
    return True


async def run(models: list[str]) -> None:
    cases = json.loads((HERE / "eval_cases.json").read_text(encoding="utf-8"))
    params = StdioServerParameters(command=sys.executable, args=[str(HERE / "server.py")])
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            tools = [mcp_to_ollama(t) for t in (await session.list_tools()).tools]
            for model in models:
                ok_tool = ok_args = 0
                t0 = time.time()
                print(f"\n== {model}")
                for c in cases:
                    msgs = [{"role": "system", "content": SYSTEM}, {"role": "user", "content": c["question"]}]
                    msg = ollama.chat(model=model, messages=msgs, tools=tools).message
                    call = msg.tool_calls[0].function if msg.tool_calls else None
                    name = call.name if call else None
                    args = dict(call.arguments or {}) if call else {}
                    expected = c["tool"] if isinstance(c["tool"], list) else [c["tool"]]
                    t_ok = name in expected
                    a_ok = t_ok and matches(c.get("arguments", {}), args)
                    ok_tool += t_ok
                    ok_args += a_ok
                    mark = "✓" if a_ok else ("~" if t_ok else "✗")
                    print(f"  {mark} {c['question'][:60]:<60} -> {name} {json.dumps(args, ensure_ascii=False)}")
                n = len(cases)
                print(f"  correct tool: {ok_tool}/{n} | correct arguments: {ok_args}/{n} "
                      f"| {time.time() - t0:.0f}s")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", action="append", default=[])
    a = ap.parse_args()
    asyncio.run(run(a.model or ["riftbound"]))
