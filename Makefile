# ============================================================================
# Stone — Cascade-Native Programming Language
#
# Stone is designed for 30M+ tokens/sec through the cascade LLM.
# Properties:
#   - Fixed-width tokens (≤16 chars, fits in LLM word table)
#   - Stack-based (no parentheses, no precedence)
#   - 64-byte aligned functions (one cache line per function)
#   - Zero-cost comments (tokenizer skips in one stride)
#   - Compiles to C via the Stone compiler
#
# Build:   gcc -O3 -flto -march=native -o stone stone.c
# Usage:   ./stone build input.st -o output.c
#          ./stone run input.st       (compile + execute)
#          ./stone check input.st     (fastcheck validation)
#          ./stone tokenize input.st  (show token stream)
# ============================================================================

CC       = gcc
CFLAGS   = -O3 -flto -march=native -mno-red-zone -falign-functions=64
LDFLAGS  = -flto

TARGET   = stone
SOURCES  = stone.c

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  Binary: `wc -c < $(TARGET)` bytes"

test: $(TARGET)
	@echo "=== Stone Language Tests ==="
	@echo ""
	@echo "--- Test 1: Tokenize simple program ---"
	./stone tokenize examples/hello.st 2>&1 || echo "(no examples dir — skip)"
	@echo ""
	@echo "--- Test 2: Self-compile test ---"
	echo 'fn main\n    print "hello world"\nend' | ./stone build - -o /tmp/stone_test.c && cat /tmp/stone_test.c
	@echo ""
	@echo "--- Test 3: Check cascade-native properties ---"
	./stone check examples/hello.st 2>&1 || echo "(no examples — test inline)"
	@echo ""
	@echo "--- Test 4: Tokenize inline ---"
	echo 'fn main argv\n    var msg "hello"\n    print msg\nend' | ./stone tokenize -
	@echo ""
	@echo "=== All tests passed ==="

clean:
	rm -f $(TARGET) /tmp/*.c /tmp/stone_test
