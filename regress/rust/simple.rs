use orb::ort;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    assert_eq!(args.len(), 2);

    let ctx = ort::connect(&args[1]).unwrap();

    let id1 = ctx.db_foo_insert
        (&"testing".to_string(), 1.2, ort::data::Enm::A, 0x01).unwrap();

    let objs1 = ctx.db_foo_get_id(id1).unwrap().unwrap();

    assert_eq!(&objs1.data.a, "testing");
    assert_eq!(objs1.data.c, ort::data::Enm::A);
    assert_eq!(objs1.data.d, 1);
    assert!(&objs1.data.b + f64::EPSILON >= 1.2 && 
            objs1.data.b - f64::EPSILON <= 1.2);
}
