fn main() {
    let mut programmer = Programmer::new(9001);
    programmer.train(1);
    println!("Power: {}. Approved: {}", programmer.power, programmer.intimidate());

    let message = Message::Move { y: 4, x: 3 };
    println!("Score: {}", score(message));
    let text = Message::Write(String::from("The compiler sensed your power."));
    match text {
        Message::Write(message) => { println!("{}", message); }
        _ => { println!("The compiler sensed hesitation."); }
    }
    println!("Quit score: {}", score(Message::Quit));
}

struct Programmer {
    power: i32,
}

impl Programmer {
    fn new(power: i32) -> Self {
        Self { power }
    }

    fn train(&mut self, amount: i32) {
        self.power += amount;
    }

    fn intimidate(&self) -> bool {
        self.power > 9000
    }
}

enum Message {
    Quit,
    Move { x: i32, y: i32 },
    Write(String),
}

fn score(message: Message) -> i32 {
    match message {
        Message::Quit => 0,
        Message::Move { x, y } => x * x + y * y,
        Message::Write(_) => 1,
    }
}
