use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new_with_args(&args[1], ort::Ortargs { bcrypt_cost: 4 });
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(&"testing".to_string(), &"foobarbaz".to_string(), ort::data::Enm::A, 0x01).unwrap();

    let obj1 = ctx.db_foo_get_id(id1).unwrap().unwrap();

    assert_eq!(obj1.export(), r#"{"a":"testing","c":"200","d":"1","id":"1"}"#);


}
