// Tests ssr_sha256 with the test vectors of FIPS 180-4.

#include <string.h>

#include "ssr_sha256.h"
#include "ssr_test.h"

static int is(const char *text, const char *want) {
    char hex[65];
    ssr_sha256_hex(text, strlen(text), hex);
    return strcmp(hex, want) == 0;
}

int main(void) {
    CHECK(is("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK(is("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    // Two blocks: the length does not fit after the 56 bytes.
    CHECK(is("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    // A million "a": many blocks.
    static char many[1000001];
    memset(many, 'a', 1000000);
    CHECK(is(many, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    return TEST_RESULT();
}
