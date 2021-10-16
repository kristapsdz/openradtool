use orb;
use std::env;

fn main() {
    let args = env::args();
    assert_eq!(args.count(), 2);
}
