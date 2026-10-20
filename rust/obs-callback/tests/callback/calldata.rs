//! Tier 1 mirror of `test/cmocka/test_calldata.c`.
//!
//! Null receivers and `bnum_allocs` stay with that cmocka test. The safe
//! core keeps its own buffer, so it has no process-wide alloc counter.
//! `calldata_create` / `calldata_destroy` are header inlines and stay C.
//! A fixed overflow is counted here; the shim turns the count into `blog`
//! of [`FIXED_OVERFLOW_MESSAGE`].

use std::mem::size_of;

use obs_callback::{Calldata, FIXED_OVERFLOW_MESSAGE, StringGet};

fn read_usize(bytes: &[u8], at: usize) -> usize {
    let n = size_of::<usize>();
    let mut tmp = [0u8; 8];
    tmp[..n].copy_from_slice(&bytes[at..at + n]);
    if n == 8 {
        u64::from_ne_bytes(tmp) as usize
    } else {
        u32::from_ne_bytes(tmp[..4].try_into().unwrap()) as usize
    }
}

/// Name length, name, data length, data, and the terminating 0.
fn param_bytes(name_len: usize, data_size: usize) -> usize {
    size_of::<usize>() * 3 + name_len + data_size
}

fn expect_param(stack: &[u8], offset: &mut usize, name: &str, data: &[u8]) {
    let name_len = name.len() + 1;
    let mut expected_name = name.as_bytes().to_vec();
    expected_name.push(0);
    let p = &stack[*offset..];
    assert_eq!(read_usize(p, 0), name_len);
    assert_eq!(
        &p[size_of::<usize>()..size_of::<usize>() + name_len],
        expected_name.as_slice()
    );
    let data_at = size_of::<usize>() + name_len;
    assert_eq!(read_usize(p, data_at), data.len());
    if !data.is_empty() {
        assert_eq!(
            &p[data_at + size_of::<usize>()..data_at + size_of::<usize>() + data.len()],
            data
        );
    }
    *offset += size_of::<usize>() + name_len + size_of::<usize>() + data.len();
}

fn expect_end(cd: &Calldata<'_>, offset: usize) {
    let stack = cd.stack().unwrap();
    assert_eq!(read_usize(stack, offset), 0);
    assert_eq!(cd.size(), offset + size_of::<usize>());
}

#[test]
fn guards_test() {
    let mut cd = Calldata::new();
    assert!(cd.stack().is_none());
    assert_eq!(cd.size(), 0);
    assert_eq!(cd.capacity(), 0);
    assert!(!cd.is_fixed());

    let out = 0x1111i64;
    assert!(cd.get_data("", size_of::<i64>()).is_none());
    assert!(matches!(cd.get_string(""), StringGet::Missing));
    assert!(matches!(cd.get_string("missing"), StringGet::Missing));
    assert_eq!(out, 0x1111);

    cd.set_data("", &out.to_ne_bytes());
    assert!(cd.stack().is_none());

    cd.free();
    assert!(cd.stack().is_none());
}

#[test]
fn first_param_test() {
    let mut cd = Calldata::new();
    let value = 42i64;
    cd.set_int("a", value);

    assert!(cd.stack().is_some());
    assert!(!cd.is_fixed());
    // The first allocation is at least 128 bytes. The used size is not.
    assert_eq!(cd.capacity(), 128);
    assert_eq!(cd.size(), param_bytes(2, size_of::<i64>()));
    assert!(cd.size() < cd.capacity());

    let mut offset = 0;
    expect_param(cd.stack().unwrap(), &mut offset, "a", &value.to_ne_bytes());
    expect_end(&cd, offset);

    assert_eq!(cd.get_int("a"), Some(value));
    assert!(cd.get_int("A").is_none());
    assert!(cd.get_int("b").is_none());

    // Name "a" leaves the payload off a size_t boundary. The bytes still
    // round-trip into an unaligned destination.
    assert_ne!((size_of::<usize>() + 2) % size_of::<usize>(), 0);
    let mut raw = [0x5Au8; size_of::<i64>() + 1];
    let got = cd.get_data("a", size_of::<i64>()).unwrap();
    raw[1..].copy_from_slice(got);
    assert_eq!(&raw[1..], &value.to_ne_bytes());
    assert_eq!(raw[0], 0x5A);

    // A wrong size does not write the output.
    let out = 7i64;
    assert!(cd.get_data("a", size_of::<i64>() - 1).is_none());
    assert_eq!(out, 7);
}

#[test]
fn capacity_boundary_test() {
    let under = 128 - size_of::<usize>() * 3 - 1;
    let exact = 128 - size_of::<usize>() * 3;
    assert!(under > 1);
    assert!(exact > 1);

    let mut name = vec![b'n'; under - 1];
    let mut cd = Calldata::new();
    cd.set_data(std::str::from_utf8(&name).unwrap(), &[]);
    assert_eq!(cd.size(), param_bytes(under, 0));
    assert_eq!(cd.size(), 127);
    assert_eq!(cd.capacity(), 128);
    cd.free();

    name = vec![b'n'; exact - 1];
    let mut cd = Calldata::new();
    cd.set_data(std::str::from_utf8(&name).unwrap(), &[]);
    assert_eq!(cd.size(), 128);
    assert_eq!(cd.capacity(), 128);
}

#[test]
fn string_test() {
    let mut cd = Calldata::new();
    let hello = "hello";
    let hello_len = hello.len() + 1;
    cd.set_string("hello", Some(hello));
    match cd.get_string("hello") {
        StringGet::Data(bytes) => {
            assert_eq!(c_text(bytes), hello);
            let stack = cd.stack().unwrap().as_ptr();
            assert_eq!(
                bytes.as_ptr(),
                stack.wrapping_add(size_of::<usize>() + hello_len + size_of::<usize>())
            );
        }
        other => panic!("expected hello, got {other:?}"),
    }
    let mut offset = 0;
    let mut hello_bytes = hello.as_bytes().to_vec();
    hello_bytes.push(0);
    expect_param(cd.stack().unwrap(), &mut offset, "hello", &hello_bytes);
    expect_end(&cd, offset);

    // Characterized, not endorsed: a null string is stored as a zero-size
    // parameter and get_string reports success with a null pointer.
    cd.set_string("none", None);
    assert_eq!(cd.get_string("none"), StringGet::Null);
    assert!(cd.string("none").is_none());

    cd.set_string("blank", Some(""));
    match cd.get_string("blank") {
        StringGet::Data(bytes) => assert_eq!(c_text(bytes), ""),
        other => panic!("expected blank, got {other:?}"),
    }

    assert_eq!(cd.get_string("missing"), StringGet::Missing);
    assert!(cd.string("missing").is_none());
}

fn c_text(bytes: &[u8]) -> &str {
    let end = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());
    std::str::from_utf8(&bytes[..end]).unwrap()
}

#[test]
fn resize_keeps_later_param_test() {
    let mut cd = Calldata::new();
    let first = 11i64;
    let second = 22i64;
    cd.set_int("a", first);
    cd.set_int("b", second);
    let stack = cd.stack().unwrap().as_ptr();
    let capacity = cd.capacity();
    let size = cd.size();

    // Same size: no move, no new allocation.
    cd.set_int("a", 99);
    assert_eq!(cd.stack().unwrap().as_ptr(), stack);
    assert_eq!(cd.capacity(), capacity);
    assert_eq!(cd.size(), size);
    assert_eq!(cd.int("a"), 99);
    assert_eq!(cd.int("b"), second);

    let grown = [0x3Cu8; 32];
    cd.set_data("a", &grown);
    assert_eq!(cd.size(), size + (grown.len() - size_of::<i64>()));
    assert_eq!(cd.get_data("a", grown.len()), Some(grown.as_slice()));
    assert_eq!(cd.int("b"), second);
    let mut offset = 0;
    expect_param(cd.stack().unwrap(), &mut offset, "a", &grown);
    expect_param(cd.stack().unwrap(), &mut offset, "b", &second.to_ne_bytes());
    expect_end(&cd, offset);

    let first = 33i64;
    cd.set_int("a", first);
    assert_eq!(cd.int("a"), first);
    assert_eq!(cd.int("b"), second);
    offset = 0;
    expect_param(cd.stack().unwrap(), &mut offset, "a", &first.to_ne_bytes());
    expect_param(cd.stack().unwrap(), &mut offset, "b", &second.to_ne_bytes());
    expect_end(&cd, offset);

    // Shrinking to an empty payload keeps the name and the later one.
    cd.set_data("a", &[]);
    assert!(cd.get_int("a").is_none());
    assert_eq!(cd.get_data("a", 0), Some(&[][..]));
    assert_eq!(cd.int("b"), second);
}

#[test]
fn exact_capacity_fits_test() {
    let mut cd = Calldata::new();
    cd.set_data("a", b"Z");
    let size = cd.size();
    assert_eq!(cd.capacity(), 128);
    assert!(size < 128);

    // An exact fit already includes the terminating size field.
    let extra = 128 - size;
    let overhead = 2 + size_of::<usize>() * 2;
    assert!(extra > overhead);
    let data_size = extra - overhead;
    let blob = vec![0x11u8; data_size];
    cd.set_data("b", &blob);

    assert_eq!(cd.size(), 128);
    assert_eq!(cd.capacity(), 128);
    assert_eq!(cd.get_data("a", 1), Some(&b"Z"[..]));
    assert_eq!(cd.get_data("b", data_size), Some(blob.as_slice()));
}

#[test]
fn growth_past_double_test() {
    let mut cd = Calldata::new();
    cd.set_int("a", 5);
    assert_eq!(cd.capacity(), 128);

    let target = 300;
    let overhead = 2 + size_of::<usize>() * 2;
    assert!(target > cd.size() + overhead);
    assert!(target > cd.capacity() * 2);
    let data_size = target - cd.size() - overhead;
    let blob = vec![0x22u8; data_size];
    cd.set_data("b", &blob);

    assert_eq!(cd.size(), target);
    assert_eq!(cd.capacity(), target);
    assert_eq!(cd.get_int("a"), Some(5));
}

#[test]
fn typed_helpers_test() {
    let mut cd = Calldata::new();
    let obj = 7i32;
    cd.set_int("n", 1);
    cd.set_float("f", 1.5);
    cd.set_bool("flag", true);
    cd.set_ptr("p", &obj as *const i32 as usize);

    assert_eq!(cd.get_int("n"), Some(1));
    assert_eq!(cd.get_float("f"), Some(1.5));
    assert_eq!(cd.get_bool("flag"), Some(true));
    assert_eq!(cd.ptr("p"), &obj as *const i32 as usize);

    assert_eq!(cd.int("missing"), 0);
    assert_eq!(cd.float("missing"), 0.0);
    assert!(!cd.bool("missing"));
    assert_eq!(cd.ptr("missing"), 0);

    // A bool is one byte, so an int get does not accept it.
    assert_eq!(cd.get_data("flag", 1), Some(&[1u8][..]));
    assert!(cd.get_int("flag").is_none());

    // Characterized, not endorsed: the header says get checks the type.
    // The stack stores only a size, and i64 and f64 are both 8 bytes, so
    // get_float succeeds on an int.
    assert_eq!(size_of::<i64>(), size_of::<f64>());
    let one = 1i64;
    let f = cd.get_float("n").unwrap();
    assert_eq!(f.to_ne_bytes(), one.to_ne_bytes());
}

#[test]
fn clear_and_reuse_test() {
    let mut cd = Calldata::new();
    cd.clear();
    assert!(cd.stack().is_none());
    assert_eq!(cd.size(), 0);

    cd.set_int("a", 11);
    cd.set_string("b", Some("hi"));
    let stack = cd.stack().unwrap().as_ptr();
    let capacity = cd.capacity();
    let saved = cd.stack().unwrap()[size_of::<usize>()];

    cd.clear();
    assert_eq!(cd.stack().unwrap().as_ptr(), stack);
    assert_eq!(cd.capacity(), capacity);
    assert_eq!(cd.size(), size_of::<usize>());
    assert_eq!(read_usize(cd.stack().unwrap(), 0), 0);
    // Characterized, not endorsed: clear zeroes the first size_t only.
    assert_eq!(cd.stack().unwrap()[size_of::<usize>()], saved);
    assert!(cd.get_int("a").is_none());
    assert!(cd.string("b").is_none());

    cd.set_int("c", 9);
    assert_eq!(cd.stack().unwrap().as_ptr(), stack);
    assert_eq!(cd.int("c"), 9);
    assert!(cd.get_int("a").is_none());
}

#[test]
fn fixed_stack_test() {
    assert_eq!(
        FIXED_OVERFLOW_MESSAGE,
        "Tried to go above fixed calldata stack size!"
    );

    let needed = param_bytes(2, size_of::<i64>());
    let mut buf = vec![0xABu8; 256];
    let mut exact = vec![0u8; needed];
    let mut room = vec![0u8; 128];
    let buf_ptr = buf.as_ptr();
    let room_ptr = room.as_ptr();

    let mut cd = Calldata::fixed(&mut buf[..needed + 1]);
    assert!(cd.is_fixed());
    assert_eq!(cd.stack().unwrap().as_ptr(), buf_ptr);
    assert_eq!(cd.capacity(), needed + 1);
    assert_eq!(cd.size(), size_of::<usize>());
    assert_eq!(read_usize(cd.stack().unwrap(), 0), 0);

    cd.set_int("a", 42);
    assert_eq!(cd.overflow_count(), 0);
    assert_eq!(cd.size(), needed);
    assert_eq!(cd.get_int("a"), Some(42));

    // A write beyond the fixed capacity is refused and preserves the value.
    let bigger = vec![7u8; size_of::<i64>() + 2];
    cd.set_data("a", &bigger);
    assert_eq!(cd.overflow_count(), 1);
    assert_eq!(cd.size(), needed);
    assert_eq!(cd.int("a"), 42);
    assert_eq!(cd.stack().unwrap()[needed], 0xAB);

    // The terminator is included in needed; an exact fit is valid.
    let mut cd_exact = Calldata::fixed(&mut exact);
    cd_exact.set_int("a", 5);
    assert_eq!(cd_exact.overflow_count(), 0);
    assert_eq!(cd_exact.size(), needed);
    assert_eq!(cd_exact.get_int("a"), Some(5));

    let mut cd_room = Calldata::fixed(&mut room);
    cd_room.set_int("a", 1);
    cd_room.set_int("b", 2);
    assert_eq!(cd_room.overflow_count(), 0);
    assert_eq!(cd_room.int("a"), 1);
    assert_eq!(cd_room.int("b"), 2);
    let mut offset = 0;
    expect_param(
        cd_room.stack().unwrap(),
        &mut offset,
        "a",
        &1i64.to_ne_bytes(),
    );
    expect_param(
        cd_room.stack().unwrap(),
        &mut offset,
        "b",
        &2i64.to_ne_bytes(),
    );
    expect_end(&cd_room, offset);

    // free of a fixed stack does not release or clear the buffer.
    cd.free();
    cd_exact.free();
    cd_room.free();
    assert_eq!(cd_room.stack().unwrap().as_ptr(), room_ptr);
    assert_eq!(cd_room.int("b"), 2);
}
