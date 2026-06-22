# Fibonacci in Stone
# Shows: variables, loop, arithmetic, function calls
# Cascade-native: ~20 tokens total
# C equivalent: ~45 tokens

fn main
    var n 10
    var a 0
    var b 1
    loop i n
        print a
        print " "
        var next a + b
        a = b
        b = next
    end
    printn ""
end
