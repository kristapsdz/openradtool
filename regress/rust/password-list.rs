use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new_with_args(&args[1], ort::Ortargs { bcrypt_cost: 4 });
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(&"password".to_string()).unwrap();
    assert_ne!(id1, -1);

    let obj2 = ctx.db_foo_list_hash(&"password".to_string()).unwrap();
    assert_eq!(obj2.len(), 1);

    let obj3 = ctx.db_foo_list_hash(&"shmassword".to_string()).unwrap();
    assert_eq!(obj3.len(), 0);

    let obj4 = ctx.db_foo_list_nhash(&"password".to_string()).unwrap();
    assert_eq!(obj4.len(), 0);

    let obj5 = ctx.db_foo_list_nhash(&"shmassword".to_string()).unwrap();
    assert_eq!(obj5.len(), 1);
}
