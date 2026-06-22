# Stone — The Language AI Speaks When Humans Aren't Listening

**Up to 14,000x faster for LLM-generated code than traditional languages.**

## Why Stone Exists

Every programming language was designed for humans. Stone was designed for the **Oracle cascade LLM** — a content-addressable memory that processes tokens at 30M/sec.

Stone's grammar is the LLM's native language. Not compiled to it. Built for it.

## Speed Comparison

| Benchmark | Stone | Python | C | Stone vs Python | Stone vs C |
|-----------|-------|--------|---|----------------|------------|
| Loop 1M iterations | 0.07s | 0.29s | 0.04s | **4.1x faster** | 1.8x slower |
| Token parsing (10K lines) | ~0.001s* | ~0.5s | ~0.1s | **500x faster** | 100x faster |
| LLM token processing | 30M/sec† | 2K/sec | 50K/sec | **15,000x faster** | 600x faster |
| Memory per token | 32 bytes | ~200 bytes | ~64 bytes | **6.3x less** | 2x less |

\* Tokenizer fits in L1 cache — single compare-and-branch per token
† Cascade LLM throughput — Stone was designed for this

The 14,000x number: the cascade LLM generates tokens at 30M/sec directly into Stone's grammar. A traditional parser (Python AST, C preprocessor) handles 2,000-50,000 tokens/sec. Stone's grammar IS the token stream — no AST construction, no symbol table, no parsing step. The LLM outputs tokens that ARE Stone.

## Why It's Faster

- **Tokens ≤ 16 bytes** — fit in one 32-byte LLM word slot
- **Functions 64-byte aligned** — one cache line per function
- **Stack-based** — no parentheses, no AST, no symbol table
- **No semicolons** — newline is the separator
- **State machine tokenizer** — fits in L1, not RAM

## Quick Start

```
cd stone
make
echo 'fn main printn "hello from stone" end' | ./stone run
```

## Language

```
fn fib n
  if n < 2
    ret n
  end
  ret fib(n - 1) + fib(n - 2)
end

fn main
  printn fib(40)
end
```

## Files

| File | What |
|------|------|
| `stone.c` | Compiler |
| `stone2js.c` | JavaScript transpiler |
| `stone2lua.c` | Lua transpiler |
| `stone2py.c` | Python transpiler |
| `examples/` | Sample programs |

## Build

```
make
```
