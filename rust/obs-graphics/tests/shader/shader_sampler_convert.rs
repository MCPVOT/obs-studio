//! Tier 1: mirrors test/cmocka/test_shader_sampler_convert.c through the safe API.
//! Native strtol stays in the adapter. Numeric fixtures supply its characterized
//! results and assert the offset/base requested by the safe core.
use obs_graphics::shader_param_names::{address, filter};
use obs_graphics::shader_sampler_convert::{Sampler, SamplerInfo, convert};

struct Fixture<'a> {
    names: &'a [&'a str],
    values: &'a [&'a str],
    numeric: &'a [i128],
}
impl Sampler for Fixture<'_> {
    fn state_count(&self) -> usize {
        self.names.len()
    }
    fn state(&self, index: usize) -> &[u8] {
        self.names[index].as_bytes()
    }
    fn value(&self, index: usize) -> &[u8] {
        self.values[index].as_bytes()
    }
    fn number(&self, index: usize, offset: usize, radix: u32) -> i128 {
        if self.names[index].eq_ignore_ascii_case("MaxAnisotropy") {
            assert_eq!((offset, radix), (0, 10));
        } else {
            assert!(self.names[index].eq_ignore_ascii_case("BorderColor"));
            assert_eq!((offset, radix), (1, 16));
        }
        self.numeric[index]
    }
}
fn run(names: &[&str], values: &[&str], numeric: &[i128]) -> SamplerInfo {
    let fixture = Fixture {
        names,
        values,
        numeric,
    };
    let original_names = names.to_vec();
    let original_values = values.to_vec();
    let names_ptr = names.as_ptr();
    let values_ptr = values.as_ptr();
    let info = convert(&fixture);
    assert_eq!(fixture.names.as_ptr(), names_ptr);
    assert_eq!(fixture.values.as_ptr(), values_ptr);
    assert_eq!(fixture.names, original_names);
    assert_eq!(fixture.values, original_values);
    assert_eq!(fixture.state_count(), names.len());
    info
}
fn defaults() -> SamplerInfo {
    SamplerInfo {
        filter: filter::POINT,
        address_u: address::CLAMP,
        address_v: address::CLAMP,
        address_w: address::CLAMP,
        max_anisotropy: 1,
        border_color: 0,
    }
}
#[test]
fn test_shader_sampler_convert_empty_and_unknown() {
    assert_eq!(run(&[], &[], &[]), defaults());
    assert_eq!(
        run(
            &[
                "",
                "Unknown",
                "Filter ",
                " AddressU",
                "MaxAnisotropyX",
                "BorderColour"
            ],
            &["Point", "Mirror", "Anisotropy", "Wrap", "12", "#123456"],
            &[]
        ),
        defaults()
    );
}

#[test]
fn test_shader_sampler_convert_filters() {
    let cases = [
        ("Point", filter::POINT),
        ("pOiNt", filter::POINT),
        ("Linear", filter::LINEAR),
        ("lInEaR", filter::LINEAR),
        ("Anisotropy", filter::ANISOTROPIC),
        ("ANISOTROPY", filter::ANISOTROPIC),
        ("MIN_MAG_MIP_POINT", filter::POINT),
        ("MIN_MAG_MIP_LINEAR", filter::LINEAR),
        ("MIN_MAG_POINT_MIP_LINEAR", filter::MIN_MAG_POINT_MIP_LINEAR),
        (
            "MIN_POINT_MAG_LINEAR_MIP_POINT",
            filter::MIN_POINT_MAG_LINEAR_MIP_POINT,
        ),
        ("MIN_POINT_MAG_MIP_LINEAR", filter::MIN_POINT_MAG_MIP_LINEAR),
        ("MIN_LINEAR_MAG_MIP_POINT", filter::MIN_LINEAR_MAG_MIP_POINT),
        (
            "MIN_LINEAR_MAG_POINT_MIP_LINEAR",
            filter::MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        ),
        ("MIN_MAG_LINEAR_MIP_POINT", filter::MIN_MAG_LINEAR_MIP_POINT),
        ("min_mag_mip_point", filter::LINEAR),
        ("", filter::LINEAR),
        ("unknown", filter::LINEAR),
        ("Point ", filter::LINEAR),
    ];
    for (text, value) in cases {
        let mut expected = defaults();
        expected.filter = value;
        assert_eq!(run(&["fIlTeR"], &[text], &[]), expected);
    }
}

#[test]
fn test_shader_sampler_convert_addresses() {
    let cases = [
        ("Wrap", address::WRAP),
        ("REPEAT", address::WRAP),
        ("Clamp", address::CLAMP),
        ("None", address::CLAMP),
        ("mIrRoR", address::MIRROR),
        ("BORDER", address::BORDER),
        ("MirrorOnce", address::MIRRORONCE),
        ("", address::CLAMP),
        ("unknown", address::CLAMP),
        ("Wrap ", address::CLAMP),
    ];
    for axis in 0..3 {
        for (text, value) in cases {
            let key = ["aDdReSsU", "ADDRESSV", "addressw"][axis];
            let mut expected = defaults();
            match axis {
                0 => expected.address_u = value,
                1 => expected.address_v = value,
                _ => expected.address_w = value,
            }
            assert_eq!(run(&[key], &[text], &[]), expected);
        }
    }
}

#[test]
fn test_shader_sampler_convert_anisotropy() {
    let cases = [
        ("0", 0),
        ("1", 1),
        ("16", 16),
        ("-7", -7),
        (" +12tail", 12),
        ("010", 10),
        ("0x10", 0),
        ("", 0),
        ("junk", 0),
        ("2147483647", i32::MAX),
        ("-2147483648", i32::MIN),
    ];
    for (text, value) in cases {
        let mut expected = defaults();
        expected.max_anisotropy = value;
        assert_eq!(
            run(&["mAxAnIsOtRoPy"], &[text], &[i128::from(value)]),
            expected
        );
    }
}

#[test]
fn test_shader_sampler_convert_border_colors() {
    let cases = [
        ("#0", 0),
        ("#123abc", 0x123abc),
        ("#7fffffff", 0x7fffffff),
        ("#aBcDeF", 0xabcdef),
        ("#0x1234", 0x1234),
        ("# +12tail", 0x12),
        ("#-1", u32::MAX),
        ("#", 0),
        ("#junk", 0),
        ("x1234", 0x1234),
        ("1234", 0x234),
    ];
    for (text, value) in cases {
        let mut expected = defaults();
        expected.border_color = value;
        let parsed = if text == "#-1" { -1 } else { i128::from(value) };
        assert_eq!(run(&["bOrDeRcOlOr"], &[text], &[parsed]), expected);
    }
}

#[test]
fn test_shader_sampler_convert_combined_and_duplicates() {
    let expected = SamplerInfo {
        filter: filter::LINEAR,
        address_u: address::MIRRORONCE,
        address_v: address::WRAP,
        address_w: address::CLAMP,
        max_anisotropy: -2,
        border_color: u32::MAX,
    };
    assert_eq!(
        run(
            &[
                "Filter",
                "AddressU",
                "AddressV",
                "AddressW",
                "MaxAnisotropy",
                "BorderColor",
                "FILTER",
                "addressu",
                "addressv",
                "addressw",
                "MAXANISOTROPY",
                "BORDERCOLOR",
                "ignored"
            ],
            &[
                "Point",
                "Wrap",
                "Mirror",
                "Border",
                "4",
                "#123456",
                "unknown",
                "MirrorOnce",
                "Repeat",
                "None",
                "-2",
                "#-1",
                "0"
            ],
            &[0, 0, 0, 0, 4, 0x123456, 0, 0, 0, 0, -2, -1, 0]
        ),
        expected
    );
    assert_eq!(run(&[], &[], &[]), defaults());
}
