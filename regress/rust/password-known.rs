use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);
    let db = ort::Ortdb::new_with_args(&args[1], ort::Ortargs { bcrypt_cost: 4 });
    let ctx = db.connect().unwrap();

    let id1 = ctx.db_foo_insert(&"password".to_string()).unwrap();
    assert_ne!(id1, -1);

    let obj1 = ctx.db_foo_get_hash(&"password".to_string()).unwrap();
    assert!(obj1.is_some());

    assert_eq!(ctx.db_foo_update_hash(&"$2b$08$YCwD6QeXPZTW8NOEuh5H5er5ALQq9.r58nqUxy8K/IVamm.h7Fl2q".to_string(), id1).unwrap(), true);

    let obj2 = ctx.db_foo_get_hash(&"foobarbaz".to_string()).unwrap();
    assert!(obj2.is_some());
}
