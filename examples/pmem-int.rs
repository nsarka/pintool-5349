use corundum::default::*;
use corundum::open_flags::*;
use std::env;

type P = Allocator;
 
#[derive(Root)]
struct Node {
    /* Why does the linkedlist example in the corundum paper work with the value
       field being an i32 and not a PRefCell<i32> like in this example? */
    //value: i32,

    value: PRefCell<i32>
}

fn main() {

    let args: Vec<String> = env::args().collect();

    match args[1].as_ref() {
        "get" => test_get("/optane/sarkauskas.1/tmp_proj1/pools/test.pool"),
        "set" => test_set("/optane/sarkauskas.1/tmp_proj1/pools/test.pool", args[2].parse::<i32>().unwrap()),
        _ => ()
    }
}

fn test_get(s: &str) {
    let root = P::open::<Node>(s, O_CFNE | O_1GB).unwrap();

    println!("Value: {}", *root.value.borrow());
    println!("Pool range: {:#01x} : {:#01x}\n", P::start(), P::end());
}

fn test_set(s: &str, x: i32) {
    // Persistent pointer
    let root = P::open::<Node>(s, O_CFNE | O_1GB).unwrap();

    // Volatile pointer
    let mut vol = Box::new(5);

    P::transaction(|j| {
        // Write to persistent pointer
        *root.value.borrow_mut(j) = x;
    }).expect("Unsuccessful transaction");

    println!("Wrote {} to pmem\n", x);

    // Write to volatile pointer
    *vol = 7;
    println!("Wrote {} to vol", vol);
}
