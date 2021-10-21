use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new(&args[1]);
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_bar_insert(&"foobar".to_string()).unwrap();
    let id2 = ctx.db_foo_insert(id1, &"testing".to_string(), 1.2, ort::data::Enm::A, 0x01).unwrap();
    let obj1 = ctx.db_foo_get_id(id2).unwrap().unwrap();

    assert_eq!(obj1.export(), r#"{"bar":{"a":"foobar","id":"1"},"barid":"1","a":"testing","b":"1.2","c":"200","d":"1","id":"1"}"#);
}
