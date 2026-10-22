//! Safe sampler-state conversion. The adapter retains the native C runtime's
//! numeric parsing (including long width, locale and errno) behind `number`.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SamplerInfo {
    pub filter: i32,
    pub address_u: i32,
    pub address_v: i32,
    pub address_w: i32,
    pub max_anisotropy: i32,
    pub border_color: u32,
}

/// Read-only sampler view. Numeric conversion is supplied by the host adapter;
/// `offset` skips the border-color prefix and `radix` is 10 or 16.
pub trait Sampler {
    fn state_count(&self) -> usize;
    fn state(&self, index: usize) -> &[u8];
    fn value(&self, index: usize) -> &[u8];
    fn number(&self, index: usize, offset: usize, radix: u32) -> i128;
}

pub fn convert(sampler: &impl Sampler) -> SamplerInfo {
    use crate::shader_param_names::{address, address_mode, filter, sample_filter};
    let mut info = SamplerInfo {
        filter: filter::POINT,
        address_u: address::CLAMP,
        address_v: address::CLAMP,
        address_w: address::CLAMP,
        max_anisotropy: 1,
        border_color: 0,
    };
    for index in 0..sampler.state_count() {
        let state = sampler.state(index);
        if state.eq_ignore_ascii_case(b"Filter") {
            info.filter = sample_filter(sampler.value(index));
        } else if state.eq_ignore_ascii_case(b"AddressU") {
            info.address_u = address_mode(sampler.value(index));
        } else if state.eq_ignore_ascii_case(b"AddressV") {
            info.address_v = address_mode(sampler.value(index));
        } else if state.eq_ignore_ascii_case(b"AddressW") {
            info.address_w = address_mode(sampler.value(index));
        } else if state.eq_ignore_ascii_case(b"MaxAnisotropy") {
            info.max_anisotropy = sampler.number(index, 0, 10) as i32;
        } else if state.eq_ignore_ascii_case(b"BorderColor") {
            info.border_color = sampler.number(index, 1, 16) as u32;
        }
    }
    info
}
