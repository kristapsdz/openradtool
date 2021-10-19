use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new_with_args(&args[1], ort::Ortargs { bcrypt_cost: 4 });
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(None).unwrap();
    assert_ne!(id1, -1);

    let obj2 = ctx.db_foo_get_hash(Some(&"password".to_string())).unwrap();
    assert!(obj2.is_none());
    let obj3 = ctx.db_foo_get_hash(Some(&"shmassword".to_string())).unwrap();
    assert!(obj3.is_none());
    let obj4 = ctx.db_foo_get_nhash(Some(&"password".to_string())).unwrap();
    assert!(obj4.is_none());
    let obj5 = ctx.db_foo_get_nhash(Some(&"shmassword".to_string())).unwrap();
    assert!(obj5.is_none());
}
