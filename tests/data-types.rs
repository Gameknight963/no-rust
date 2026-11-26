fn main() {
    let mut counter = Counter::new(10);
    charge(&mut counter);
    counter.add(2);
    let outer = Outer { inner: counter };
    println!("{} {}", outer.inner.value, outer.inner.get());
    println!("{}", Counter::answer());
    let pair = Pair { right: 8, left: 5 };
    println!("{} {}", pair.left, pair.right);

    let first: Event = Event::First(7);
    let second = Event::Second(9);
    println!("{} {}", first.score(), second.score());
    println!("{}", Event::Point { y: 4, x: 3 }.score());
    println!("{}", Event::Empty.score());
    println!("{}", Event::Text(String::from("hello")).score());
    println!("{}", Event::Pair(2, 6).score());
    println!("{}", Event::Zero().score());
    println!("{}", Event::NamedEmpty {}.score());

    let result = match (Event::Point { x: 3, y: 5 }) {
        Event::Point { y: height, .. } => {
            let extra = 2;
            height + extra
        }
        _ => 0,
    };
    println!("{}", result);
    match Event::First(11) {
        Event::First(value) => { println!("{}", value); }
        _ => { println!("bad"); }
    }
    println!("{}", early(Event::First(12)));
    println!("{}", early(Event::Empty));
    println!("{}", named_ignore(Event::Point { x: 1, y: 2 }));

    for i in 0..3 {
        match Event::First(i) {
            Event::First(value) => {
                if value == 1 { break; }
                println!("loop {}", value);
            }
            _ => {}
        }
    }
    let literal = Point { x: 3, y: 4 };
    if literal.x == 3 { println!("fields work"); }
    let nested = Event::First(match Event::Empty { Event::Empty => 20, _ => 0 });
    println!("{}", nested.score());
    println!("{}", Color::Green.score());
    println!("{}", literal_match());
    println!("{}", ordered_fields());
}

struct Outer { inner: Counter }
struct Point { x: i32, y: i32 }
struct Pair { left: i32, right: i32 }

impl Counter {
    fn new(value: i32) -> Self { Self { value } }
    fn add(&mut self, value: i32) { self.value += value; }
    fn get(&self) -> i32 { self.value }
    fn answer() -> i32 { 42 }
}

struct Counter { value: i32 }

fn charge(counter: &mut Counter) { counter.value += 1; }

enum Event {
    Empty,
    First(i32),
    Second(i32),
    Point { x: i32, y: i32 },
    Text(String),
    Pair(i32, i32),
    Zero(),
    NamedEmpty {},
}

impl Event {
    fn score(&self) -> i32 {
        match self {
            Self::Empty => 0,
            Event::First(value) => *value,
            Event::Second(value) => value * 10,
            Event::Point { x, y } => x + y,
            Event::Text(text) => { println!("{}", text); 100 }
            Event::Pair(left, right) => left * right,
            Event::Zero() => 200,
            Event::NamedEmpty {} => 300,
        }
    }
}

fn early(event: Event) -> i32 {
    match event {
        Event::First(value) => { return value; }
        _ => {}
    }
    99
}

fn named_ignore(event: Event) -> i32 {
    match event {
        Event::Point { x: _, y } => y,
        _ => 0,
    }
}

enum Color { Red, Green, Blue }
impl Color {
    fn score(&self) -> i32 {
        match self {
            Color::Red => 1,
            Color::Green => 2,
            Color::Blue => 3,
        }
    }
}

fn literal_match() -> i32 {
    match (Event::Point { y: 30, x: 10 }) {
        Event::Point { x, y } => x + y,
        _ => 0,
    }
}

fn increment(value: &mut i32) -> i32 {
    *value += 1;
    *value
}

fn ordered_fields() -> i32 {
    let mut next = 0;
    let pair = Pair { right: increment(&mut next), left: increment(&mut next) };
    pair.left * 10 + pair.right
}
