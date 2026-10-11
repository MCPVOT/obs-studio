#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "librtmp/amf.h"

/* AMF_GetProp hands back a pointer to the shared AMFProp_Invalid sentinel
 * when the property is not present, never NULL, so a caller must check
 * AMFProp_IsValid rather than comparing against NULL. */
static void inner_property_lookup(void **state)
{
	(void)state;

	const char doc[] = {
		AMF_AVMPLUS, AMF3_OBJECT, 0x0b, 0x01, 0x03,         'a',  AMF3_OBJECT, AMF3_OBJECT,
		0x0b,        0x01,        0x03, 'a',  AMF3_INTEGER, 0x00, 0x01,        0x01,
	};
	AMFObject obj;
	AMFObjectProperty *prop;
	AMFObject inner;
	AVal a = AVC("a");

	assert_int_equal(AMF_Decode(&obj, doc, (int)sizeof(doc), FALSE), (int)sizeof(doc));

	prop = AMF_GetProp(&obj, NULL, 0);
	assert_true(AMFProp_IsValid(prop));
	assert_int_equal(AMFProp_GetType(prop), AMF_OBJECT);
	AMFProp_GetObject(prop, &inner);

	prop = AMF_GetProp(&inner, &a, -1);
	assert_true(AMFProp_IsValid(prop));
	assert_int_equal(AMFProp_GetType(prop), AMF_OBJECT);

	/* a missing property still returns the sentinel, not NULL */
	prop = AMF_GetProp(&inner, &(AVal)AVC("nope"), -1);
	assert_false(AMFProp_IsValid(prop));

	AMF_Reset(&obj);
}

/* A spec-valid flat dynamic object: the trait says one sealed member "a"
 * followed by a single dynamic member, and the empty name that closes the
 * member list must not be stored as a property of its own. */
static void flat_dynamic_object_terminator(void **state)
{
	(void)state;

	/* A spec-valid flat dynamic object: a trait with no sealed members and
	 * dynamic set, one dynamic member "a", then the empty name that closes
	 * the member list. This is the document the AMF3 spec actually describes,
	 * pinned so a future change that starts storing the terminator as a
	 * property is caught. The use-after-free itself is reproduced by the
	 * decoder-compatible nested document below. */
	const char doc[] = {
		AMF_AVMPLUS,  AMF3_OBJECT, 0x0b, /* no sealed members, dynamic */
		0x05,         'k',         0,    /* inline class name "k", NUL-terminated */
		0x03,         'a',               /* dynamic member name "a" */
		AMF3_INTEGER, 0x01,              /* member "a" = 1 */
		0x01,                            /* empty name closes the member list */
	};
	AMFObject obj;
	int count;

	assert_int_equal(AMF_Decode(&obj, doc, (int)sizeof(doc), FALSE), (int)sizeof(doc));

	/* the terminator is a list delimiter, so exactly one property is kept */
	count = AMF_CountProp(&obj);
	assert_int_equal(count, 1);

	AMF_Reset(&obj);
}

/* A decoder-compatible document that reproduces the use-after-free: librtmp
 * reads a nested object's marker twice — once as the member type and once
 * as the object header — so each nested level emits it twice, and on master
 * the empty-name terminator was added with the stale union pointer
 * inherited from member "a". This is not a spec-valid document; it exists
 * to drive the decoder as real servers drive it. */
static void nested_dynamic_object(void **state)
{
	(void)state;

	const char doc[] = {
		AMF_AVMPLUS, AMF3_OBJECT, 0x0b, 0x01, 0x03,         'a',  AMF3_OBJECT, AMF3_OBJECT,
		0x0b,        0x01,        0x03, 'a',  AMF3_INTEGER, 0x00, 0x01,        0x01,
	};
	AMFObject obj;
	AMFObjectProperty *prop;
	AMFObject inner;
	AVal a = AVC("a");
	int outer_count;

	assert_int_equal(AMF_Decode(&obj, doc, (int)sizeof(doc), FALSE), (int)sizeof(doc));

	outer_count = AMF_CountProp(&obj);
	prop = AMF_GetProp(&obj, NULL, 0);
	assert_true(AMFProp_IsValid(prop));
	assert_int_equal(AMFProp_GetType(prop), AMF_OBJECT);
	AMFProp_GetObject(prop, &inner);

	AMF_Reset(&obj);

	/* the empty-name terminator is a list delimiter, not a property */
	assert_int_equal(outer_count, 1);
	assert_int_equal(AMF_CountProp(&inner), 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(inner_property_lookup),
		cmocka_unit_test(flat_dynamic_object_terminator),
		cmocka_unit_test(nested_dynamic_object),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
