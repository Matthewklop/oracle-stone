# Conditional logic in Stone
# Shows: if/else, comparison operators
# Cascade-native: ~12 tokens
# C equivalent: ~28 tokens

fn main
    var score 85
    if score > 90
        printn "A"
    else
        if score > 80
            printn "B"
        else
            printn "C"
        end
    end
end
