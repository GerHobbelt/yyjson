#include "yyjson.h"
#include "yy_test_utils.h"


#if !YYJSON_DISABLE_WRITER

static bool mut_val_has_inf_nan(yyjson_mut_val *val) {
    usize idx, max;
    yyjson_mut_val *k, *v;
    
    if (yyjson_mut_is_real(val)) {
        f64 num = yyjson_mut_get_real(val);
        if (isnan(num) || isinf(num)) return true;
        return false;
    }
    if (yyjson_mut_is_arr(val)) {
        yyjson_mut_arr_foreach(val, idx, max, v) {
            if (mut_val_has_inf_nan(v)) return true;
        }
    }else if (yyjson_mut_is_obj(val)) {
        yyjson_mut_obj_foreach(val, idx, max, k, v) {
            if (mut_val_has_inf_nan(v)) return true;
        }
    }
    return false;
}

static usize mut_val_get_num(yyjson_mut_val *val) {
    usize idx, max, num;
    yyjson_mut_val *k, *v;
    
    if (!val) return 0;
    if (!yyjson_mut_is_ctn(val)) return 1;
    
    num = 1;
    if (yyjson_mut_is_arr(val)) {
        yyjson_mut_arr_foreach(val, idx, max, v) {
            num += mut_val_get_num(v);
        }
    }else if (yyjson_mut_is_obj(val)) {
        yyjson_mut_obj_foreach(val, idx, max, k, v) {
            num += 1;
            num += mut_val_get_num(v);
        }
    }
    return num;
}


static void validate_json_write_with_flag(yyjson_write_flag flg,
                                          yyjson_mut_doc *doc,
                                          yyjson_alc *alc,
                                          const char *expect) {
#if !YYJSON_DISABLE_READER
    // write mutable doc to string
    usize len;
    char *ret = yyjson_mut_write_opts(doc, flg, alc, &len, NULL);
    if (!expect) {
        yy_assertf(!ret && len == 0, "write with flag 0x%x\nexpect fail, but return:\n%s\n", flg, ret);
        return;
    }
    yy_assertf(ret && len > 0, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(strlen(ret) == len, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(strlen(expect) == len, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(memcmp(ret, expect, len) == 0, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    
    
    // write mutable doc to file
    const char *tmp_file_path = "__yyjson_test_tmp__.json";
    yy_file_delete(tmp_file_path);
    yy_assert(yyjson_mut_write_file(tmp_file_path, doc, flg, alc, NULL));
    u8 *dat;
    usize dat_len;
    yy_assert(yy_file_read(tmp_file_path, &dat, &dat_len));
    yy_assert(dat_len == len);
    yy_assert(memcmp(dat, ret, len) == 0);
    free(dat);
    yy_file_delete(tmp_file_path);
    
    
    // read
    yyjson_read_flag rflg = YYJSON_READ_NOFLAG;
    if (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) rflg |= YYJSON_READ_ALLOW_INF_AND_NAN;
    yyjson_doc *idoc = yyjson_read_opts(ret, len, rflg, NULL, NULL);
    yy_assert(idoc);
    if (mut_val_get_num(doc->root) != idoc->val_read) {
        idoc = yyjson_read_opts(ret, len, rflg, NULL, NULL);
    }
    yy_assert(mut_val_get_num(doc->root) == idoc->val_read);
    
    
    // write immutable doc to string
    usize len2;
    char *ret2 = yyjson_write_opts(idoc, flg, NULL, &len2, NULL);
    yy_assert(len == len2 && ret2);
    yy_assert(memcmp(ret, ret2, len) == 0);
    
    
    // write immutable doc to file
    yy_file_delete(tmp_file_path);
    yy_assert(yyjson_write_file(tmp_file_path, idoc, flg, alc, NULL));
    u8 *dat2;
    usize dat2_len;
    yy_assert(yy_file_read(tmp_file_path, &dat2, &dat2_len));
    yy_assert(dat2_len == len);
    yy_assert(memcmp(dat2, ret, len) == 0);
    free(dat2);
    yy_file_delete(tmp_file_path);
    
    
    // copy mutable doc and write again
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
    yy_assert(mdoc);
    usize len3;
    char *ret3 = yyjson_mut_write_opts(doc, flg, NULL, &len3, NULL);
    yy_assert(len == len3 && ret3);
    yy_assert(memcmp(ret, ret3, len) == 0);
    
    
    yyjson_doc_free(idoc);
    free(ret2);
    yyjson_mut_doc_free(mdoc);
    free(ret3);
    
    if (alc) alc->free(alc->ctx, (void *)ret);
    else free((void *)ret);
#endif
}

static void validate_json_write(yyjson_mut_doc *doc,
                                yyjson_alc *alc,
                                const char *min,
                                const char *pre) {
    yyjson_write_flag flg;
    bool has_nan_inf = mut_val_has_inf_nan(yyjson_mut_doc_get_root(doc));
    if (!pre) pre = min;
    
    // nan inf should fail without 'ALLOW_INF_AND_NAN' flag
    if (has_nan_inf) {
        flg = YYJSON_WRITE_NOFLAG;
        validate_json_write_with_flag(flg, doc, alc, NULL);
        flg = YYJSON_WRITE_PRETTY;
        validate_json_write_with_flag(flg, doc, alc, NULL);
    }
    
    // minify
    flg = YYJSON_WRITE_NOFLAG;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN;
    validate_json_write_with_flag(flg, doc, alc, min);
    
    // pretty
    flg = YYJSON_WRITE_PRETTY;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN;
    validate_json_write_with_flag(flg, doc, alc, pre);
    
    // use small allocator to test allocation failure
    if (min && pre && alc && strlen(min) > 8) {
        char buf[64];
        yyjson_alc small_alc;
        yyjson_alc_pool_init(&small_alc, buf, 8 * sizeof(void *));
        for (int i = 1; i < 64; i++) small_alc.malloc(small_alc.ctx, i);
        validate_json_write(doc, &small_alc, NULL, NULL);
    }
}

static void test_json_write(yyjson_alc *alc) {
    
    yyjson_mut_doc *doc;
    yyjson_mut_val *root, *val, *val2;
    char *str1, *str2, *cur1, *cur2;
    usize len;
    yyjson_write_err err;
    
    doc = yyjson_mut_doc_new(NULL);
    
    
    // invalid params
    yy_assert(!yyjson_mut_write(NULL, 0, NULL));
    yy_assert(!yyjson_mut_write(doc, 0, NULL));
    
    len = 1;
    yy_assert(!yyjson_mut_write(NULL, 0, &len));
    yy_assert(len == 0);
    len = 1;
    yy_assert(!yyjson_mut_write(doc, 0, &len));
    yy_assert(len == 0);
    
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, NULL, NULL));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL));
    
    len = 1;
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, &len, NULL));
    yy_assert(len == 0);
    len = 1;
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, &len, NULL));
    yy_assert(len == 0);
    
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, NULL, &err));
    yy_assert(err.code && err.msg);
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, NULL, &err));
    yy_assert(err.code && err.msg);
    
    len = 1;
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, &len, &err));
    yy_assert(len == 0);
    yy_assert(err.code && err.msg);
    len = 1;
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, &len, &err));
    yy_assert(len == 0);
    yy_assert(err.code && err.msg);
    
    
    // invalid
    root = yyjson_mut_null(doc);
    root->tag = 0;
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, NULL, NULL);
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_null(doc);
    val->tag = 0;
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc, NULL, NULL);
    
    
    // single
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    root = yyjson_mut_real(doc, NAN);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "NaN", NULL);
    
    root = yyjson_mut_real(doc, -INFINITY);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "-Infinity", NULL);
#endif
    
    root = yyjson_mut_null(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "null", NULL);
    
    root = yyjson_mut_true(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "true", NULL);
    
    root = yyjson_mut_false(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "false", NULL);
    
    root = yyjson_mut_uint(doc, 123);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "123", NULL);
    
    root = yyjson_mut_sint(doc, -123);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "-123", NULL);
    
    root = yyjson_mut_real(doc, -1.5);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "-1.5", NULL);
    
    root = yyjson_mut_str(doc, "abc");
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "\"abc\"", NULL);
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "[]", NULL);
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "{}", NULL);

    
    // array
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_int(doc, root, 1);
    validate_json_write(doc, alc,
                        "[1]",
                        "[\n"
                        "    1\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_int(doc, root, 1);
    yyjson_mut_arr_add_int(doc, root, 2);
    validate_json_write(doc, alc,
                        "[1,2]",
                        "[\n"
                        "    1,\n"
                        "    2\n"
                        "]");
    
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_real(doc, root, NAN);
    validate_json_write(doc, alc,
                        "[NaN]",
                        "[\n"
                        "    NaN\n"
                        "]");
#endif
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_str(doc, root, "abc");
    yyjson_mut_arr_add_bool(doc, root, true);
    yyjson_mut_arr_add_null(doc, root);
    validate_json_write(doc, alc,
                        "[\"abc\",true,null]",
                        "[\n"
                        "    \"abc\",\n"
                        "    true,\n"
                        "    null\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc,
                        "[[]]",
                        "[\n"
                        "    []\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_arr(doc, val);
    validate_json_write(doc, alc,
                        "[[[]]]",
                        "[\n"
                        "    [\n"
                        "        []\n"
                        "    ]\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc,
                        "[{}]",
                        "[\n"
                        "    {}\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, root);
    validate_json_write(doc, alc,
                        "[[],true]",
                        "[\n"
                        "    [],\n"
                        "    true\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_true(doc, root);
    yyjson_mut_arr_add_arr(doc, root);
    validate_json_write(doc, alc,
                        "[true,[]]",
                        "[\n"
                        "    true,\n"
                        "    []\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, val);
    validate_json_write(doc, alc,
                        "[[true]]",
                        "[\n"
                        "    [\n"
                        "        true\n"
                        "    ]\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, val);
    val = yyjson_mut_arr_add_arr(doc, root);
    validate_json_write(doc, alc,
                        "[[true],[]]",
                        "[\n"
                        "    [\n"
                        "        true\n"
                        "    ],\n"
                        "    []\n"
                        "]");
    
    cur1 = str1 = malloc(1024 * 2 + 4);
    cur2 = str2 = malloc(1024 * 7 + 4);
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    *cur1++ = '[';
    *cur2++ = '[';
    *cur2++ = '\n';
    for (int i = 0; i < 1024; i++) {
        yyjson_mut_arr_add_int(doc, root, 1);
        memcpy(cur1, "1,", 2);
        cur1 += 2;
        memcpy(cur2, "    1,\n", 7);
        cur2 += 7;
    }
    cur1 -= 1;
    *cur1++ = ']';
    *cur1 = '\0';
    cur2 -= 2;
    *cur2++ = '\n';
    *cur2++ = ']';
    *cur2 = '\0';
    validate_json_write(doc, alc, str1, str2);
    free(str1);
    free(str2);
    
    
    // object
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    yyjson_mut_obj_add_int(doc, root, "abc", 123);
    yy_assert(doc->root);
    validate_json_write(doc, alc,
                        "{\"abc\":123}",
                        "{\n"
                        "    \"abc\": 123\n"
                        "}");
    
#if !YYJSON_DISABLE_INF_AND_NAN_READER
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    yyjson_mut_obj_add_real(doc, root, "abc", NAN);
    yy_assert(doc->root);
    validate_json_write(doc, alc,
                        "{\"abc\":NaN}",
                        "{\n"
                        "    \"abc\": NaN\n"
                        "}");
#endif
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "abc", val);
    yy_assert(doc->root);
    validate_json_write(doc, alc,
                        "{\"abc\":{}}",
                        "{\n"
                        "    \"abc\": {}\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_null(doc, root, "a");
    yyjson_mut_obj_add_true(doc, root, "b");
    yyjson_mut_obj_add_int(doc, root, "c", 123);
    yyjson_mut_obj_add_str(doc, root, "d", "zzz");
    validate_json_write(doc, alc,
                        "{\"a\":null,\"b\":true,\"c\":123,\"d\":\"zzz\"}",
                        "{\n"
                        "    \"a\": null,\n"
                        "    \"b\": true,\n"
                        "    \"c\": 123,\n"
                        "    \"d\": \"zzz\"\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_null(doc, root, "a");
    yyjson_mut_obj_add_true(doc, root, "a");
    yyjson_mut_obj_add_false(doc, root, "a");
    yyjson_mut_obj_add_int(doc, root, "a", 123);
    yyjson_mut_obj_add_str(doc, root, "a", "zzz");
    validate_json_write(doc, alc,
                        "{\"a\":null,\"a\":true,\"a\":false,\"a\":123,\"a\":\"zzz\"}",
                        "{\n"
                        "    \"a\": null,\n"
                        "    \"a\": true,\n"
                        "    \"a\": false,\n"
                        "    \"a\": 123,\n"
                        "    \"a\": \"zzz\"\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, val, 123);
    yyjson_mut_obj_add_val(doc, root, "a", val);
    validate_json_write(doc, alc,
                        "{\"a\":[123]}",
                        "{\n"
                        "    \"a\": [\n"
                        "        123\n"
                        "    ]\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "a", val);
    val2 = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, val, "b", val2);
    validate_json_write(doc, alc,
                        "{\"a\":{\"b\":{}}}",
                        "{\n"
                        "    \"a\": {\n"
                        "        \"b\": {}\n"
                        "    }\n"
                        "}");
    
    cur1 = str1 = malloc(1024 * 6 + 32);
    cur2 = str2 = malloc(1024 * 12 + 32);
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    *cur1++ = '{';
    *cur2++ = '{';
    *cur2++ = '\n';
    for (int i = 0; i < 1024; i++) {
        yyjson_mut_obj_add_int(doc, root, "a", 1);
        memcpy(cur1, "\"a\":1,", 6);
        cur1 += 6;
        memcpy(cur2, "    \"a\": 1,\n", 12);
        cur2 += 12;
    }
    cur1 -= 1;
    *cur1++ = '}';
    *cur1 = '\0';
    cur2 -= 2;
    *cur2++ = '\n';
    *cur2++ = '}';
    *cur2 = '\0';
    validate_json_write(doc, alc, str1, str2);
    free(str1);
    free(str2);
    
    yyjson_mut_doc_free(doc);
}

yy_test_case(test_json_writer) {
    // test read and roundtrip
    yyjson_alc alc;
    usize len = 1024 * 1024;
    void *buf = malloc(len);
    yyjson_alc_pool_init(&alc, buf, len);
    test_json_write(&alc);
    test_json_write(NULL);
    free(buf);
    
    // test invalid parameters
    yyjson_write_file(NULL, NULL, 0, NULL, NULL);
    yyjson_write_file("", NULL, 0, NULL, NULL);
    yyjson_write_file("tmp.json", NULL, 0, NULL, NULL);
    yyjson_mut_write_file(NULL, NULL, 0, NULL, NULL);
    yyjson_mut_write_file("", NULL, 0, NULL, NULL);
    yyjson_mut_write_file("tmp.json", NULL, 0, NULL, NULL);
    
    // test invalid immutable doc
    yyjson_doc *doc = yyjson_read("[1]", 3, 0);
    yy_assert(doc);
    yyjson_val *arr = yyjson_doc_get_root(doc);
    yy_assert(arr);
    yyjson_val *one = yyjson_arr_get(arr, 0);
    yy_assert(one);
    one->tag = YYJSON_TYPE_NONE;
    yy_assert(!yyjson_write(doc, 0, NULL));
    yy_assert(!yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL));
    one->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL;
    one->uni.f64 = NAN;
    yy_assert(!yyjson_write(doc, 0, NULL));
    yy_assert(!yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL));
    yyjson_doc_free(doc);
    
    
#if !YYJSON_DISABLE_READER
    // test fail
    char path[4100];
    memset(path, 'a', sizeof(path));
    path[4099] = '\0';
    yyjson_doc *idoc = yyjson_read("1", 1, 0);
    yy_assert(!yyjson_write_file(path, idoc, 0, NULL, NULL));
    yyjson_doc_free(idoc);
    
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_doc_set_root(mdoc, yyjson_mut_null(mdoc));
    yy_assert(!yyjson_mut_write_file(path, mdoc, 0, NULL, NULL));
    yyjson_mut_doc_free(mdoc);
#endif
}

#else
yy_test_case(test_json_writer) {}
#endif
