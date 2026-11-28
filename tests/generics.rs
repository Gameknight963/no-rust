fn main() {
    println!("{} {}", identity(17), identity::<i32>(42));
    println!("{}", identity::<String>(String::from("still C++")));
    let pair = Pair { second: 8, first: 3 };
    println!("{} {}", pair.first, pair.second);
    let typed: Pair<i32> = Pair::<i32> { first: 10, second: 20 };
    println!("{}", first(typed));
    let nested: Pair<Pair<i32>> = Pair::<Pair<i32>> {
        first: Pair::<i32> { first: 1, second: 2 },
        second: Pair::<i32> { first: 3, second: 4 },
    };
    println!("{}", nested.second.first);
    let mixed = Mixed::<i32, String> { left: 5, right: String::from("hello") };
    println!("{} {}", mixed.left, mixed.right);
    println!("{}", forward(23));
    let holder = Holder { pair: Pair::<i32> { first: 6, second: 7 } };
    println!("{}", holder.pair.second);
    println!("{}", first::<String>(Pair::<String> {
        first: String::from("templates"), second: String::from("everywhere")
    }));
    println!("{}", pass_array::<i32>([11, 12])[1]);
}

struct Holder { pair: Pair<i32> }
struct Pair<T> { first: T, second: T }
struct Mixed<A, B> { left: A, right: B }

fn forward<T>(value: T) -> T { identity(value) }
fn identity<T>(value: T) -> T { value }
fn first<T>(pair: Pair<T>) -> T { pair.first }
fn pass_array<T>(values: [T; 2]) -> [T; 2] { values }
