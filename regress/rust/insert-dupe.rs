use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new(&args[1]);
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(100).unwrap();
    assert_ne!(id1, -1);
    let id2 = ctx.db_foo_insert(100).unwrap();
    assert_eq!(id2, -1);
}
