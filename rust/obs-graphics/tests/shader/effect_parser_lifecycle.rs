//! Tier 1 mirrors test_effect_parser_lifecycle.c through the safe core.
//! Tracked values stand for owned C allocations; borrowed values outlive free.

use obs_graphics::effect_parser_lifecycle::{self as lifecycle, Collection, Parser};
use std::cell::Cell;
use std::rc::Rc;

struct Allocation(Rc<Cell<usize>>);
impl Allocation {
    fn new(count: &Rc<Cell<usize>>) -> Self {
        count.set(count.get() + 1);
        Self(count.clone())
    }
}
impl Drop for Allocation {
    fn drop(&mut self) {
        self.0.set(self.0.get() - 1);
    }
}

#[derive(Default)]
struct Element {
    owned: Vec<Allocation>,
    nested: Vec<Element>,
    borrowed: Vec<Rc<Cell<usize>>>,
}

impl Element {
    fn owned(count: &Rc<Cell<usize>>, allocations: usize) -> Self {
        Self {
            owned: (0..allocations).map(|_| Allocation::new(count)).collect(),
            ..Self::default()
        }
    }
}

#[derive(Default)]
struct Fixture {
    arrays: [Vec<Option<Element>>; 5],
    storage: [Option<Allocation>; 5],
    current_pass: Option<usize>,
    parser_state: Option<Element>,
    current_token: Option<usize>,
    effect: Option<usize>,
    files: Vec<Allocation>,
    tokens: Vec<Allocation>,
    events: Vec<String>,
}

impl Fixture {
    fn add(&mut self, collection: Collection, element: Element, count: &Rc<Cell<usize>>) {
        let i = collection as usize;
        self.storage[i].get_or_insert_with(|| Allocation::new(count));
        self.arrays[i].push(Some(element));
    }
    fn assert_cleared(&self) {
        for array in &self.arrays {
            assert!(array.is_empty());
            assert_eq!(array.capacity(), 0);
        }
        assert!(self.storage.iter().all(Option::is_none));
        assert!(self.current_pass.is_none());
        assert!(self.current_token.is_none());
        assert!(self.parser_state.is_none());
    }
    fn finish(&mut self, count: &Cell<usize>, retained: usize) {
        lifecycle::free(self);
        self.assert_cleared();
        assert_eq!(count.get(), retained);
        lifecycle::free(self);
        self.assert_cleared();
        assert_eq!(count.get(), retained);
    }
}

impl Parser for Fixture {
    fn len(&self, collection: Collection) -> usize {
        self.arrays[collection as usize].len()
    }
    fn destroy_element(&mut self, collection: Collection, index: usize) {
        self.events.push(format!("element {collection:?} {index}"));
        let element = self.arrays[collection as usize][index].take().unwrap();
        // Inspect the fixture's ownership graph without changing its Drop behavior.
        let _ = (
            element.owned.len(),
            element.nested.len(),
            element.borrowed.len(),
        );
        drop(element);
    }
    fn clear_current_pass(&mut self) {
        self.events.push("pass".into());
        self.current_pass = None;
    }
    fn destroy_parser_state(&mut self) {
        self.events.push("parser".into());
        self.parser_state = None;
        self.current_token = None;
    }
    fn release_storage(&mut self, collection: Collection) {
        self.events.push(format!("storage {collection:?}"));
        self.arrays[collection as usize] = Vec::new();
        self.storage[collection as usize] = None;
    }
}

#[test]
fn empty_cleanup() {
    Fixture::default().finish(&Cell::new(0), 0);
}

#[test]
fn nested_parameters() {
    let count = Rc::new(Cell::new(0));
    let mut parser = Fixture::default();
    for _ in 0..2 {
        let mut parameter = Element::owned(&count, 4);
        let mut annotation = Element::owned(&count, 4);
        annotation.nested.push(Element::owned(&count, 3));
        parameter.nested.push(annotation);
        parser.add(Collection::Parameters, parameter, &count);
    }
    parser.finish(&count, 0);
}

#[test]
fn structures() {
    let count = Rc::new(Cell::new(0));
    let mut parser = Fixture::default();
    for _ in 0..2 {
        let mut item = Element::owned(&count, 2);
        item.nested = (0..2).map(|_| Element::owned(&count, 3)).collect();
        parser.add(Collection::Structures, item, &count);
    }
    parser.finish(&count, 0);
}

#[test]
fn functions_and_borrowed_dependencies() {
    let count = Rc::new(Cell::new(0));
    let dependency = Allocation::new(&count);
    let mut parser = Fixture::default();
    for _ in 0..2 {
        let mut function = Element::owned(&count, 9);
        function.nested = (0..2).map(|_| Element::owned(&count, 3)).collect();
        function.borrowed = vec![dependency.0.clone(); 8];
        parser.add(Collection::Functions, function, &count);
    }
    parser.finish(&count, 1);
    assert_eq!(Rc::strong_count(&dependency.0), 2);
    drop(dependency);
    assert_eq!(count.get(), 0);
}

#[test]
fn samplers() {
    let count = Rc::new(Cell::new(0));
    let mut parser = Fixture::default();
    for _ in 0..2 {
        parser.add(Collection::Samplers, Element::owned(&count, 7), &count);
    }
    parser.finish(&count, 0);
}

#[test]
fn techniques_and_passes() {
    let count = Rc::new(Cell::new(0));
    let borrowed_pass = Allocation::new(&count);
    let mut parser = Fixture {
        current_pass: Some(42),
        ..Fixture::default()
    };
    for _ in 0..2 {
        let mut technique = Element::owned(&count, 2);
        for _ in 0..2 {
            let mut pass = Element::owned(&count, 3);
            pass.borrowed.push(borrowed_pass.0.clone());
            technique.nested.push(pass);
        }
        parser.add(Collection::Techniques, technique, &count);
    }
    parser.finish(&count, 1);
    assert_eq!(Rc::strong_count(&borrowed_pass.0), 2);
    drop(borrowed_pass);
    assert_eq!(count.get(), 0);
}

#[test]
fn parser_state() {
    let count = Rc::new(Cell::new(0));
    let mut parser = Fixture {
        parser_state: Some(Element::owned(&count, 12)),
        current_token: Some(3),
        ..Fixture::default()
    };
    parser.finish(&count, 0);
}

#[test]
fn retained_fields_and_borrowed_properties() {
    let count = Rc::new(Cell::new(0));
    let effect = Allocation::new(&count);
    let effect_param = Allocation::new(&count);
    let property = Allocation::new(&count);
    let mut parser = Fixture {
        effect: Some(42),
        files: vec![Allocation::new(&count)],
        tokens: vec![Allocation::new(&count)],
        ..Fixture::default()
    };
    let mut parameter = Element::owned(&count, 4);
    parameter.borrowed = vec![effect_param.0.clone(), property.0.clone()];
    parser.add(Collection::Parameters, parameter, &count);
    parser.finish(&count, 5);
    assert_eq!(parser.effect, Some(42));
    assert_eq!(parser.files.len(), 1);
    assert_eq!(parser.tokens.len(), 1);
    assert_eq!(Rc::strong_count(&property.0), 6);
    drop((parser, effect, effect_param, property));
    assert_eq!(count.get(), 0);
}

#[test]
fn destruction_order() {
    let count = Rc::new(Cell::new(0));
    let mut parser = Fixture::default();
    for collection in lifecycle::COLLECTIONS {
        for _ in 0..2 {
            parser.add(collection, Element::owned(&count, 1), &count);
        }
    }
    lifecycle::free(&mut parser);
    let mut expected = Vec::new();
    for collection in lifecycle::COLLECTIONS {
        for index in 0..2 {
            expected.push(format!("element {collection:?} {index}"));
        }
    }
    expected.extend(["pass".into(), "parser".into()]);
    expected.extend(lifecycle::COLLECTIONS.map(|c| format!("storage {c:?}")));
    assert_eq!(parser.events, expected);
    parser.assert_cleared();
    assert_eq!(count.get(), 0);
}
