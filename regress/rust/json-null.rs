use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new(&args[1]);
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(None, None, None, None).unwrap();

    let obj1 = ctx.db_foo_get_id(id1).unwrap().unwrap();

    assert_eq!(obj1.export(), r#"{"a":null,"b":null,"c":null,"d":null,"id":"1"}"#);


}
