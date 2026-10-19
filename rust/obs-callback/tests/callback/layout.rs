//! `struct calldata` matches the layout the C compiler gives
//! `libobs/callback/calldata.h`.

use core::mem::{align_of, offset_of, size_of};

use obs_c_oracle::calldata as c;
use obs_callback::ffi::calldata::calldata;

#[test]
fn calldata_layout_matches_c_header() {
    // SAFETY: the oracle layout functions take no arguments and return constants.
    unsafe {
        assert_eq!(size_of::<calldata>(), c::oracle_sizeof_calldata());
        assert_eq!(align_of::<calldata>(), c::oracle_alignof_calldata());
        assert_eq!(
            offset_of!(calldata, stack),
            c::oracle_offsetof_calldata_stack()
        );
        assert_eq!(
            offset_of!(calldata, size),
            c::oracle_offsetof_calldata_size()
        );
        assert_eq!(
            offset_of!(calldata, capacity),
            c::oracle_offsetof_calldata_capacity()
        );
        assert_eq!(
            offset_of!(calldata, fixed),
            c::oracle_offsetof_calldata_fixed()
        );
    }
}
