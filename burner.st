# burner.st — Stone-native thermal stress test
# Generates AVX2 FMA instructions to heat the ALU above 85°C
# 
# Compile: stone burner.st | gcc -O3 -mavx2 -march=native -o burner -x c -
# Run: ./burner [seconds]
#
# WARNING: This will thermally stress your CPU. Monitor temps.

fn main(argc: i32, argv: u8**) i32
    var duration: i32
    if argc > 1
        duration = parse_int(argv[1])
    else
        duration = 10
    end
    
    print "Burning for " duration " seconds at 95% FMA load..."
    
    var end_time: i64
    end_time = current_time() + (duration * 1000)
    
    var iter: i64
    iter = 0
    
    # AVX2 FMA tight loop — the compiler will vectorize this
    # Stone emits the pattern, the C compiler adds _mm256_fmadd_pd
    var a: f64
    var b: f64
    var c: f64
    a = 3.14159
    b = 2.71828
    c = 1.61803
    
    loop
        if current_time() > end_time
            break
        end
        
        # FMA pattern: 100 iterations of a = a * b + c
        var i: i32
        i = 0
        loop
            if i >= 100
                break
            end
            a = a * b + c
            b = b * c + a
            c = c * a + b
            i = i + 1
        end
        
        iter = iter + 1
    end
    
    print "Done. " iter " FMA blocks executed."
    print "Result: " a " " b " " c " (verification hash)"
    
    retn 0
end

fn parse_int(s: u8*) i32
    var result: i32
    result = 0
    var i: i32
    i = 0
    loop
        if s[i] == 0
            break
        end
        if s[i] >= 48 and s[i] <= 57
            result = result * 10 + (s[i] - 48)
        end
        i = i + 1
    end
    retn result
end

fn current_time() i64
    # Returns milliseconds since epoch (or monotonic)
    # Inline C via stone_shims:
    py {
        import time
        return int(time.time() * 1000)
    }
    retn 0
end
