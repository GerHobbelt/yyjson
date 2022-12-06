#include "yyjson.h"
#include "yy_test_utils.h"
#include <locale.h>

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wformat"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat"
#endif


#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER

/*==============================================================================
 * Number converter
 *============================================================================*/

#include "david_gay_dtoa.h"

/** Structure used to avoid type-based aliasing rule. */
typedef union {
    f64 f;
    u64 u;
} f64_uni;

/** Convert double to raw. */
static yy_inline u64 f64_to_u64_raw(f64 f) {
    f64_uni uni;
    uni.f = f;
    return uni.u;
}

/** Convert raw to double. */
static yy_inline f64 f64_from_u64_raw(u64 u) {
    f64_uni uni;
    uni.u = u;
    return uni.f;
}

/** Get the number of significant digit from a valid floating number string. */
static yy_inline int f64_str_get_digits(const char *str) {
    const char *cur = str, *dot = NULL, *hdr = NULL, *end = NULL;
    for (; *cur && *cur != 'e' && *cur != 'E' ; cur++) {
        if (*cur == '.') dot = cur;
        else if ('0' < *cur && *cur <= '9') {
            if (!hdr) hdr = cur;
            end = cur;
        }
    }
    if (!hdr) return 0;
    return (int)((end - hdr + 1) - (hdr < dot && dot < end));
}

/** Get random double. */
static yy_inline f64 rand_f64(void) {
    while (true) {
        u64 u = yy_random64();
        f64 f = f64_from_u64_raw(u);
        if (isfinite(f)) return f;
    };
}


/*==============================================================================
 * Test single number (uint)
 *============================================================================*/

static void test_uint_read(const char *line, usize len, u64 num) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc = yyjson_read(line, len, 0);
    yyjson_val *val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_uint(val),
               "num should be read as uint: %s\n", line);
    
    u64 get = yyjson_get_uint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %llu\nexpect: %llu\n",
               line, get, num);
    
    yyjson_doc_free(doc);
    
    /* read number as raw */
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "uint num read as raw not match:\nstr: %s\nreturn: %llu\n",
               line, yyjson_get_raw(val));
    
    yyjson_doc_free(doc);
#endif
}

static void test_uint_write(const char *line, u64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_uint(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    usize out_len;
    char *out_str;
    out_str = yyjson_mut_write(doc, 0, &out_len);
    yy_assertf(strcmp(out_str, line) == 0,
               "uint num write not match:\nstr: %s\nreturn: %s\nexpect: %s\n",
               line, out_str, line);
    
    free(out_str);
    yyjson_mut_doc_free(doc);
#endif
}

static void test_uint(const char *line, usize len) {
    u64 num = strtoull(line, NULL, 10);
    test_uint_read(line, len, num);
    
    char buf[32];
    snprintf(buf, 32, "%llu%c", num, '\0');
    test_uint_write(buf, num);
}



/*==============================================================================
 * Test single number (sint)
 *============================================================================*/

static void test_sint_read(const char *line, usize len, i64 num) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc = yyjson_read(line, len, 0);
    yyjson_val *val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_sint(val),
               "num should be read as uint: %s\n", line);
    
    i64 get = yyjson_get_sint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %lld\nexpect: %lld\n",
               line, get, num);
    
    yyjson_doc_free(doc);
    
    /* read number as raw */
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "sint num read as raw not match:\nstr: %s\nreturn: %llu\n",
               line, yyjson_get_raw(val));
    
    yyjson_doc_free(doc);
#endif
}

static void test_sint_write(const char *line, i64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_sint(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    yy_assertf(strcmp(str, line) == 0,
               "uint num write not match:\nreturn: %s\nexpect: %s\n",
               str, line);
    free(str);
    yyjson_mut_doc_free(doc);
#endif
}

static void test_sint(const char *line, usize len) {
    i64 num = strtoll(line, NULL, 10);
    test_sint_read(line, len, num);
    
    char buf[32];
    snprintf(buf, 32, "%lld%c", num, '\0');
    test_sint_write(buf, num);
    num = (i64)((u64)~num + 1); /* num = -num, avoid ubsan */
    snprintf(buf, 32, "%lld%c", num, '\0');
    test_sint_write(buf, num);
}



/*==============================================================================
 * Test single number (real)
 *============================================================================*/

static void test_real_read(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_READER
    f64 ret;
    yyjson_doc *doc;
    yyjson_val *val;
    if (isinf(num)) {
        doc = yyjson_read(line, len, 0);
        val = yyjson_doc_get_root(doc);
        yy_assertf(!doc, "num %s should fail, but returns %.17g\n",
                   line, yyjson_get_real(val));
        
        doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
        val = yyjson_doc_get_root(doc);
        yy_assertf(yyjson_is_raw(val),
                   "num should be read as raw: %s\n", line);
        yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
                   "num read as raw not match:\nstr: %s\nreturn: %llu\n",
                   line, yyjson_get_raw(val));
        yyjson_doc_free(doc);
        
#if !YYJSON_DISABLE_NON_STANDARD
        doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
        val = yyjson_doc_get_root(doc);
        ret = yyjson_get_real(val);
        yy_assertf(yyjson_is_real(val) && (ret == num),
                   "num %s should be read as inf, but returns %.17g\n",
                   line, ret);
        yyjson_doc_free(doc);
#endif
        
    } else {
        u64 num_raw = f64_to_u64_raw(num);
        u64 ret_raw;
        i64 ulp;
        
#if !YYJSON_DISABLE_FAST_FP_CONV
        // 0 ulp error
        doc = yyjson_read(line, len, 0);
        val = yyjson_doc_get_root(doc);
        ret = yyjson_get_real(val);
        ret_raw = f64_to_u64_raw(ret);
        ulp = (i64)num_raw - (i64)ret_raw;
        if (ulp < 0) ulp = -ulp;
        yy_assertf(yyjson_is_real(val) && ulp == 0,
                   "string %s should be read as %.17g, but returns %.17g\n",
                   line, num, ret);
        yyjson_doc_free(doc);
#else
        // strtod, should be 0 ulp error, but may get 1 ulp error in some env
        doc = yyjson_read(line, len, 0);
        val = yyjson_doc_get_root(doc);
        ret = yyjson_get_real(val);
        ret_raw = f64_to_u64_raw(ret);
        ulp = (i64)num_raw - (i64)ret_raw;
        if (ulp < 0) ulp = -ulp;
        yy_assertf(yyjson_is_real(val) && ulp <= 1,
                   "string %s should be read as %.17g, but returns %.17g\n",
                   line, num, ret);
        yyjson_doc_free(doc);
#endif

    }
#endif
}

static void test_real_write(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_real(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    char *str_nan_inf = yyjson_mut_write(doc, YYJSON_WRITE_ALLOW_INF_AND_NAN, NULL);
    
    if (isnan(num) || isinf(num)) {
        yy_assertf(str == NULL,
                   "num write should return NULL, but returns: %s\n",
                   str);
#if !YYJSON_DISABLE_NON_STANDARD
        yy_assertf(strcmp(str_nan_inf, line) == 0,
                   "num write not match:\nexpect: %s\nreturn: %s\n",
                   line, str_nan_inf);
#else
        yy_assertf(str_nan_inf == NULL,
                   "num write should return NULL, but returns: %s\n",
                   str);
#endif
    } else {
        yy_assert(strcmp(str, str_nan_inf) == 0);
        yy_assertf(strtod_gay(str, NULL) == num,
                   "real number write value not match:\nexpect: %s\nreturn: %s\n",
                   line, str);
        
#if !YYJSON_DISABLE_FAST_FP_CONV
        yy_assertf(f64_str_get_digits(str) == f64_str_get_digits(line),
                   "real number write value not shortest:\nexpect: %s\nreturn: %s\n",
                   line, str);
#endif
    }
    
    yyjson_mut_doc_free(doc);
    if (str) free(str);
    if (str_nan_inf) free(str_nan_inf);
    
#endif
}

static void test_real(const char *line, usize len) {
    char *end = NULL;
    f64 gay = strtod_gay(line, &end);
    yy_assertf(end && len == (usize)(end - line) && !isnan(gay), "strtod_gay fail: %s\n", line);
    test_real_read(line, len, gay);
    
    char buf[32];
    end = dtoa_gay(gay, buf);
    test_real_write(buf, end - buf, gay);
}



/*==============================================================================
 * Test single number (nan/inf)
 *============================================================================*/

static void test_nan_inf_read(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_READER
    f64 ret;
    yyjson_doc *doc;
    yyjson_val *val;
    
    /* read fail */
    doc = yyjson_read(line, len, 0);
    yy_assertf(doc == NULL, "number %s should fail in default mode\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    yy_assertf(doc == NULL, "number %s should fail in raw mode\n", line);
    
    /* read allow */
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
    val = yyjson_doc_get_root(doc);
    ret = yyjson_get_real(val);
    yy_assertf(yyjson_is_real(val), "nan or inf read fail: %s \n", line);
    if (isnan(num)) {
        yy_assertf(isnan(ret), "num %s should read as nan\n", line);
    } else {
        yy_assertf(ret == num, "num %s should read as inf\n", line);
    }
    yyjson_doc_free(doc);
    
    /* read raw */
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN | YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "num read as raw not match:\nstr: %s\nreturn: %llu\n",
               line, yyjson_get_raw(val));
    yyjson_doc_free(doc);
    
#endif
}

static void test_nan_inf_write(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_real(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    char *str_nan_inf = yyjson_mut_write(doc, YYJSON_WRITE_ALLOW_INF_AND_NAN, NULL);
    
    yy_assertf(str == NULL,
               "num write should return NULL, but returns: %s\n",
               str);
    yy_assertf(strcmp(str_nan_inf, line) == 0,
               "num write not match:\nexpect: %s\nreturn: %s\n",
               line, str_nan_inf);
    
    yyjson_mut_doc_free(doc);
    if (str) free(str);
    if (str_nan_inf) free(str_nan_inf);
#endif
}

static void test_nan_inf(const char *line, usize len) {
    char *end = NULL;
    f64 gay = strtod_gay(line, &end);
    yy_assertf(end && len == (usize)(end - line), "strtod_gay fail: %s\n", line);
    yy_assert(isnan(gay) || isinf(gay));
    
    test_nan_inf_read(line, len, gay);
    
    char buf[32];
    end = dtoa_gay(gay, buf);
    if (isnan(gay)) memcpy(buf, "NaN\0", 4);
    test_nan_inf_write(buf, end - buf, gay);
}



/*==============================================================================
 * Test single number (fail)
 *============================================================================*/

static void test_fail(const char *line, usize len) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc;
    doc = yyjson_read(line, len, 0);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW | YYJSON_READ_ALLOW_INF_AND_NAN);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
#endif
}



/*==============================================================================
 * Test with input file
 *============================================================================*/

typedef enum {
    NUM_TYPE_FAIL,
    NUM_TYPE_SINT,
    NUM_TYPE_UINT,
    NUM_TYPE_REAL,
    NUM_TYPE_INF_NAN_LITERAL,
} num_type;

static void test_with_file(const char *name, num_type type) {
    char path[YY_MAX_PATH];
    yy_path_combine(path, YYJSON_TEST_DATA_PATH, "data", "num", name, NULL);
    yy_dat dat;
    bool file_suc = yy_dat_init_with_file(&dat, path);
    yy_assertf(file_suc == true, "file read fail: %s\n", path);
    
    usize len;
    const char *line;
    while ((line = yy_dat_copy_line(&dat, &len))) {
        if (len && line[0] != '#') {
            if (type == NUM_TYPE_FAIL) {
                test_fail(line, len);
            } else if (type == NUM_TYPE_UINT) {
                test_uint(line, len);
            } else if (type == NUM_TYPE_SINT) {
                test_sint(line, len);
            } else if (type == NUM_TYPE_REAL) {
                test_real(line, len);
            } else if (type == NUM_TYPE_INF_NAN_LITERAL) {
#if !YYJSON_DISABLE_NON_STANDARD
                test_nan_inf(line, len);
#endif
            }
        }
        free((void *)line);
    }
    
    yy_dat_release(&dat);
}



/*==============================================================================
 * Test with random value
 *============================================================================*/

static void test_random_int(void) {
    char buf[32];
    char *end;
    int count = 10000;
    
    yy_random_reset();
    for (int i = 0; i < count; i++) {
        u64 rnd = yy_random64();
        end = buf + snprintf(buf, 32, "%llu%c", rnd, '\0') - 1;
        test_uint(buf, end - buf);
    }
    
    yy_random_reset();
    for (int i = 0; i < count; i++) {
        i64 rnd = (i64)(yy_random64() | ((u64)1 << 63));
        end = buf + snprintf(buf, 32, "%lld%c", rnd, '\0') - 1;
        test_sint(buf, end - buf);
    }
    
    yy_random_reset();
    for (int i = 0; i < count; i++) {
        u32 rnd = yy_random32();
        end = buf + snprintf(buf, 32, "%u%c", rnd, '\0') - 1;
        test_uint(buf, end - buf);
    }
    
    yy_random_reset();
    for (int i = 0; i < count; i++) {
        i32 rnd = (i32)(yy_random32() | ((u32)1 << 31));
        end = buf + snprintf(buf, 32, "%i%c", rnd, '\0') - 1;
        test_sint(buf, end - buf);
    }
}

static void test_random_real(void) {
    char buf[32];
    char *end;
    int count = 10000;
    
    yy_random_reset();
    for (int i = 0; i < count; i++) {
        f64 rnd = rand_f64();
        if (isinf(rnd) || isnan(rnd)) continue;
        end = dtoa_gay(rnd, buf);
        if (!yy_str_contains(buf, "e") && !yy_str_contains(buf, ".")) {
            *end++ = '.';
            *end++ = '0';
            *end = '\0';
        }
        test_real(buf, end - buf);
    }
}


/*==============================================================================
 * Test all
 *============================================================================*/

static void test_number_locale(void) {
    test_with_file("num_fail.txt", NUM_TYPE_FAIL);
    test_with_file("uint_pass.txt", NUM_TYPE_UINT);
    test_with_file("sint_pass.txt", NUM_TYPE_SINT);
    test_with_file("uint_bignum.txt", NUM_TYPE_REAL);
    test_with_file("sint_bignum.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_1.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_2.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_3.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_4.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_5.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_6.txt", NUM_TYPE_REAL);
    test_with_file("nan_inf_literal_pass.txt", NUM_TYPE_INF_NAN_LITERAL);
    test_with_file("nan_inf_literal_fail.txt", NUM_TYPE_FAIL);
    test_random_int();
    test_random_real();
}

yy_test_case(test_number) {
    setlocale(LC_ALL, "C");
    test_number_locale();
    setlocale(LC_ALL, "fr_FR");
    test_number_locale();
}


#else
yy_test_case(test_number) {}
#endif


#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif
