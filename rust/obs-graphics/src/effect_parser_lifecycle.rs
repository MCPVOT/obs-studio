//! Cleanup orchestration for the effect parser. The header-inline element
//! destructors remain C; the adapter supplies those ownership operations.

/// Owned top-level collections, in the original C destruction order.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Collection {
    Parameters,
    Structures,
    Functions,
    Samplers,
    Techniques,
}

pub const COLLECTIONS: [Collection; 5] = [
    Collection::Parameters,
    Collection::Structures,
    Collection::Functions,
    Collection::Samplers,
    Collection::Techniques,
];

/// An exclusively borrowed initialized parser. Element destruction leaves
/// collection lengths/storage intact until `release_storage` is called.
pub trait Parser {
    fn len(&self, collection: Collection) -> usize;
    fn destroy_element(&mut self, collection: Collection, index: usize);
    fn clear_current_pass(&mut self);
    fn destroy_parser_state(&mut self);
    fn release_storage(&mut self, collection: Collection);
}

/// Release owned parser state, preserving borrowed effect pointers and the
/// unused top-level files/tokens arrays, exactly as `ep_free` does.
pub fn free(parser: &mut impl Parser) {
    for collection in COLLECTIONS {
        for index in 0..parser.len(collection) {
            parser.destroy_element(collection, index);
        }
    }
    parser.clear_current_pass();
    parser.destroy_parser_state();
    for collection in COLLECTIONS {
        parser.release_storage(collection);
    }
}
