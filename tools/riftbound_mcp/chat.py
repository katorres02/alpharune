"""Terminal chat with a local model (Ollama) that uses the Riftbound MCP tools.

Usage:
    python3 chat.py                  # default model: riftbound (see Modelfile)
    python3 chat.py --model qwen3.5:9b
    python3 chat.py --log            # save every conversation to logs/ (for tuning)

Inside the chat, type `/help` to list the tools without calling the model,
and `exit` (or `quit`, `salir`) to leave.
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

HERE = Path(__file__).resolve().parent

SYSTEM = (HERE / "system_prompt.md").read_text(encoding="utf-8")


def mcp_to_ollama(tool) -> dict:
    return {
        "type": "function",
        "function": {
            "name": tool.name,
            "description": tool.description or "",
            "parameters": getattr(tool, "input_schema", None) or getattr(tool, "inputSchema", None) or {"type": "object", "properties": {}},
        },
    }


def result_text(result) -> str:
    parts = []
    for block in result.content or []:
        parts.append(getattr(block, "text", None) or str(block))
    text = "\n".join(parts)
    return text[:12000]  # keep a small model's context from overflowing


def print_help(tools) -> None:
    print("\nAvailable tools:")
    for t in tools:
        schema = getattr(t, "input_schema", None) or getattr(t, "inputSchema", None) or {}
        props = schema.get("properties", {})
        required = set(schema.get("required", []))
        args = ", ".join(p if p in required else f"[{p}]" for p in props)
        first_line = (t.description or "").strip().splitlines()[0] if t.description else ""
        print(f"  {t.name}({args})\n      {first_line}")
    print("\nAsk the model to call `help` for domain colors, set codes and the deck format.\n")


async def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="riftbound")
    ap.add_argument("--log", action="store_true")
    ap.add_argument("--max-steps", type=int, default=8)
    args = ap.parse_args()

    params = StdioServerParameters(command=sys.executable, args=[str(HERE / "server.py")])
    log_dir = HERE / "logs"
    if args.log:
        log_dir.mkdir(exist_ok=True)

    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            tools = (await session.list_tools()).tools
            ollama_tools = [mcp_to_ollama(t) for t in tools]
            print(f"Model: {args.model} | tools: {', '.join(t.name for t in tools)}")
            print("Type your question, `/help` for the tool list, or `exit`.\n")

            messages = [{"role": "system", "content": SYSTEM}]
            while True:
                try:
                    q = input("you> ").strip()
                except (EOFError, KeyboardInterrupt):
                    break
                if q.lower() in {"exit", "quit", "salir"}:
                    break
                if q.lower() in {"/help", "/h", "/?"}:
                    print_help(tools)
                    continue
                if not q:
                    continue
                messages.append({"role": "user", "content": q})

                for _ in range(args.max_steps):
                    resp = ollama.chat(model=args.model, messages=messages, tools=ollama_tools)
                    msg = resp.message
                    messages.append(msg.model_dump(exclude_none=True))
                    if not msg.tool_calls:
                        print(f"\nmodel> {msg.content}\n")
                        break
                    for call in msg.tool_calls:
                        name, arguments = call.function.name, dict(call.function.arguments or {})
                        print(f"  [tool] {name}({json.dumps(arguments, ensure_ascii=False)})")
                        try:
                            out = result_text(await session.call_tool(name, arguments))
                        except Exception as e:  # noqa: BLE001
                            out = json.dumps({"error": str(e)})
                        messages.append({"role": "tool", "tool_name": name, "content": out})
                else:
                    print("\nmodel> (reached the maximum number of tool steps)\n")

            if args.log and len(messages) > 1:
                f = log_dir / f"chat_{time.strftime('%Y%m%d-%H%M%S')}.json"
                f.write_text(json.dumps(messages, ensure_ascii=False, indent=2, default=str), encoding="utf-8")
                print(f"Conversation saved to {f}")


if __name__ == "__main__":
    asyncio.run(main())
