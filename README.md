# Stone — The Language AI Speaks When Humans Aren't Listening

**A programming language where every token fits in 32 bytes and every function aligns to a cache line. Designed for 30M+ tokens/sec inference, not human readability.**

Every programming language ever created was designed for humans. Python prioritizes readability. C prioritizes control. Rust prioritizes safety. All of them assume the reader is a person with eyeballs, a prefrontal cortex, and the patience to parse syntax.

Stone is different. Stone is designed for the reader that will consume 99% of all code in the future: the LLM.

An LLM doesn't care about indentation style. It doesn't need semicolons to understand statement boundaries. It doesn't benefit from parentheses when it already understands stack semantics natively. What an LLM needs is **predictable token boundaries** — every symbol the same width, every function on a cache line, every instruction aligned to the hardware's natural fetch unit.

Stone is what happens when you optimize a language for the machine that reads it, not the human that wrote it.

---

## Core innovation: LLM-native tokenization

Every token in Stone is ≤ 16 characters, meaning it fits in the Oracle cascade LLM's **32-byte word slot**. The LLM doesn't need to parse variable-length tokens or guess where one symbol ends and the next begins. Every token is exactly the right size for a single attention head to process in one step.

Functions are padded to **64-byte boundaries** — exactly one cache line on modern x86_64 processors. When the LLM loads a function, it loads exactly what it needs and nothing else. No cache misses. No wasted prefetch bandwidth.

Stack-based evaluation means no parentheses, no precedence rules, no syntax tree. The LLM evaluates left to right, pushing and popping as it goes. This is how LLMs already think — one token at a time, building a stack of meaning.

---

## What's here

| File | Purpose |
|------|---------|
| `stone.c` | Main compiler — compiles Stone to C. Also handles `run`, `check`, `tokenize`, and `bench` commands. |
| `stone_build.c` | Build system helper for multi-file Stone projects. |
| `stone_meta.c` | Meta-programming utilities for Stone. |
| `stone2js.c` | Transpiles Stone to JavaScript. |
| `stone2lua.c` | Transpiles Stone to Lua. |
| `stone2py.c` | Transpiles Stone to Python. |
| `examples/` | Example Stone programs. |
| `burner.st` | Bootstrapping example — Stone compiling Stone. |

---

## Language features

| Feature | Syntax | Why it's LLM-optimal |
|---------|--------|---------------------|
| **Function definition** | `fn main ... end` | No scope ambiguity. Every function starts and ends with a 32-byte token. |
| **Stack evaluation** | `1 2 +` | No parentheses. The LLM evaluates left-to-right, exactly like autoregressive token prediction. |
| **Variables** | `var x 42` | 16-char max identifier. Fits in one word slot. |
| **Conditionals** | `if cond ... else ... end` | Clear boundaries. The LLM knows exactly where each branch starts and ends. |
| **Loops** | `loop ... end` | Bounded by `loop`/`end` — a single cache line each. |
| **Output** | `print` / `printn` | One token. One instruction. |
| **Inline Python** | `py { ... }` | Escape hatch for when the LLM needs human-language code. |
| **Inline C** | `#[ ... ]` | Direct hardware access when Stone isn't fast enough. |

---

## How to build

```sh
make
```

Or manually:

```sh
gcc -O3 -o stone stone.c -lm
gcc -O3 -o stone_build stone_build.c -lm
gcc -O3 -o stone_meta stone_meta.c -lm
gcc -O3 -o stone2js stone2js.c -lm
gcc -O3 -o stone2lua stone2lua.c -lm
gcc -O3 -o stone2py stone2py.c -lm
```

---

## How to run

### Hello from Stone

Write `hello.st`:
```
fn main
  printn "hello from stone"
end
```

Run it:
```sh
./stone run hello.st
```

### Build to C

```sh
./stone build hello.st -o hello.c
gcc -O3 -o hello hello.c -lm
./hello
```

### Check syntax only

```sh
./stone check hello.st
```

### See the token breakdown

```sh
./stone tokenize hello.st
```

Shows every token and its byte alignment. Every token should fit in 32 bytes.

### Benchmark compilation speed

```sh
./stone bench hello.st
```

Measures how fast the Stone compiler processes tokens — designed for 30M+ tokens/sec throughput.

### Transpile to another language

```sh
./stone2js hello.st
./stone2lua hello.st
./stone2py hello.st
```

---

## The big picture

In the coming years, most code will not be written by humans. It will be written by LLMs — at 30 million tokens per second, operating on a global scale that no human programmer can match.

But LLMs still generate code in languages designed for humans. They produce Python with careful indentation. They write Rust with proper lifetimes. They generate C with correct pointer arithmetic. And every time they do, they waste tokens on syntax that only a human would need.

Stone is the first language that doesn't waste those tokens. Every character serves a purpose. Every token fits in a word slot. Every function is a cache line.

This is what AI sounds like when it's talking to itself. Not Python. Not C. Stone.
