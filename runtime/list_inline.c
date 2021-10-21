#include "balrt.h"

int64_t BAL_LANG_ARRAY_NAME(length)(TaggedPtr p) {
    ListPtr lp = taggedToPtr(p);
    return lp->tpArray.length;
}

void BAL_LANG_ARRAY_NAME(push)(TaggedPtr p, TaggedPtr val) {
    ListPtr lp = taggedToPtr(p);
    ListDescPtr ldp = lp->desc;
    uint32_t bitSet = ldp->bitSet;
    if ((bitSet & (1 << (getTag(val) & UT_MASK))) == 0) {
        _bal_panic_internal(storePanicCode(p, PANIC_LIST_STORE));
    }
    int64_t len = lp->tpArray.length;
    if (unlikely(len >= lp->tpArray.capacity)) {
        _bal_array_grow(&(lp->gArray), 0, TAGGED_PTR_SHIFT);
    }
    // note that array_grow does not change length
    lp->tpArray.members[len] = val;
    lp->tpArray.length = len + 1;
}

bool _bal_list_has_type(TaggedPtr p, uint32_t desc) {
    if ((getTag(p) & UT_MASK) != TAG_LIST_RW) {
        return false;
    }
    ListPtr lp = taggedToPtr(p);
    ListDescPtr ldp = lp->desc;
    return (ldp->bitSet & ~desc) == 0;
}

TaggedPtr _bal_list_exactify(TaggedPtr p, ListDescPtr desc) {
    ListPtr lp = taggedToPtr(p);
    ListDescPtr ldp = lp->desc;
    if (lp == taggedToPtrExact(p) && (ldp->bitSet == desc->bitSet)) {
        // exact bit is not set, but should be
        return p + EXACT_FLAG;
    }
    return p;
}

int64_t READONLY _bal_array_int_compare(TaggedPtr lhs, TaggedPtr rhs) {
    return intArrayCompare(lhs, rhs);
}

int64_t READONLY _bal_array_float_compare(TaggedPtr lhs, TaggedPtr rhs) {
    return floatArrayCompare(lhs, rhs);
}

int64_t READONLY _bal_array_string_compare(TaggedPtr lhs, TaggedPtr rhs) {
    return stringArrayCompare(lhs, rhs);
}

int64_t READONLY _bal_array_boolean_compare(TaggedPtr lhs, TaggedPtr rhs) {
    return booleanArrayCompare(lhs, rhs);
}
