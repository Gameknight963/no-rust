fn main() {
    let mut values: [i32; 3] = [2, 4, 6];
    values[1] += 3;
    println!("{} {} {}", values.len(), values[1], values[2]);
    let repeated = [9; 4];
    println!("{} {}", repeated.len(), repeated[3]);
    let empty = [0; 0];
    println!("{}", empty.len());
    let nested = [[1, 2], [3, 4]];
    println!("{}", nested[1][0]);
    println!("{}", choose(-1));
    println!("{}", choose(0));
    println!("{}", choose(1));
    let chosen = if values[0] == 2 {
        let offset = 10;
        if repeated[0] == 9 { offset + 1 } else { offset + 2 }
    } else { 0 };
    println!("{}", chosen);
    let mut sum = 0;
    for i in 0..=4 {
        if i % 2 == 0 { continue; }
        sum += i;
    }
    println!("{}", sum);
    let mut count = 0;
    loop {
        count += 1;
        if count < 3 { continue; }
        break;
    }
    println!("{}", count);
    let mut nested_count = 0;
    for i in 0..=2 {
        let mut j = 0;
        while j < 3 {
            j += 1;
            if j == 1 { continue; }
            nested_count += 1;
        }
        if i == 1 { continue; }
        nested_count += 10;
    }
    println!("{}", nested_count);
    let mut exclusive = 0;
    for i in 0..4 {
        if i == 1 { continue; }
        exclusive += i;
    }
    println!("{}", exclusive);
    println!("{}", make_array()[2]);
    let custom = Collection { length: 17 };
    println!("{}", custom.len());
}

struct Collection { length: i32 }
impl Collection {
    fn len(&self) -> i32 { self.length }
}

fn choose(n: i32) -> i32 {
    if n < 0 { -10 }
    else if n == 0 { 0 }
    else { 10 }
}

fn make_array() -> [i32; 3] { [5, 6, 7] }
