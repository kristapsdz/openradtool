use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new(&args[1]);
    let ctx = db.connect().unwrap();

    let val = 
        (1 << (ort::data::Bar::A as i64 - 1) | 
         1 << (ort::data::Bar::B as i64 - 1));

    let id1 = ctx.db_foo_insert(val).unwrap();

    let objs1 = ctx.db_foo_get_id(id1).unwrap().unwrap();

    assert_ne!(objs1.data.a & (1 << (ort::data::Bar::A as i64 - 1)), 0);
    assert_ne!(objs1.data.a & (1 << (ort::data::Bar::B as i64 - 1)), 0);
    assert_eq!(objs1.data.a & (1 << (ort::data::Bar::C as i64 - 1)), 0);
}
