// This is now the C++ compiler's problem.
fn main() {
    let mut total: i32 = 0;
    for i in 1..=5 {
        total += square(i);
    }
    println!("The sum of squares is {}", total);
    let mut countdown = 3;
    while countdown > 0 {
        println!("{}...", countdown);
        countdown -= 1;
    }
    if total == 55 {
        println!("Your code passed the borrow checker. We fixed that.");
    } else {
        println!("The compiler sensed hesitation.");
    }
}

fn square(n: i32) -> i32 {
    n * n
}
