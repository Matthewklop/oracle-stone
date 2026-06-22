# Stone — Cascade-Native Programming Language

A stack-based language where every token fits in 32 bytes. Designed for 30M+ tokens/sec through the Oracle cascade LLM.

## Quick Start

```
cd stone
make
echo 'fn main printn "hello from stone" end' | ./stone run
```

## What it is

Stone is a programming language where:
- **No parentheses** for grouping (stack-based evaluation)
- **No semicolons** (newline is the separator)
- **Every token ≤ 16 characters** (fits in the cascade LLM's 32-byte word slot)
- **Functions padded to 64-byte boundaries** (one cache line)
- **Compiles to C** for maximum performance

## Commands

| Command | What it does |
|---------|-------------|
| `./stone build hello.st -o hello.c` | Compile Stone to C |
| `./stone run hello.st` | Compile and run immediately |
| `./stone check hello.st` | Check syntax only |
| `./stone tokenize hello.st` | Show token breakdown |
| `./stone bench hello.st` | Benchmark compilation speed |

## Example

Write a file `hello.st`:
```
fn main
  printn "Hello from Stone!"
end
```

Run it:
```
./stone run hello.st
```

## Language Features

- `fn` — define a function
- `var` — declare a variable
- `if` / `else` / `end` — conditionals
- `loop` / `end` — loops
- `print` / `printn` — output
- `ret` — return from function
- `py { }` — inline Python blocks
- Inline C code via `#[ ... ]`

## Transpilers

Stone can also transpile to:
- `stone2js` — JavaScript
- `stone2lua` — Lua
- `stone2py` — Python

## Build

```
make
```

Or manually:
```
gcc -O3 -o stone stone.c -lm
```

## Files

| File | Purpose |
|------|---------|
| `stone.c` | Main compiler |
| `stone_build.c` | Build system helper |
| `stone_meta.c` | Meta-programming |
| `stone2js.c` | JavaScript transpiler |
| `stone2lua.c` | Lua transpiler |
| `stone2py.c` | Python transpiler |
| `examples/` | Example Stone programs |
| `burner.st` | Bootstrapping example |
