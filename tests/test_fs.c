// Tests ssr_fs with files that the test writes.

#include <stdlib.h>
#include <string.h>

#include "ssr_fs.h"
#include "ssr_test.h"
#include "ssr_zipwriter.h"

static void test_finds_a_name_in_another_case(void) {
    const char *directory = ssr_test_directory("fs");
    char command[1300];
    snprintf(command, sizeof(command), "mkdir -p '%s/NewGUI/Screens' && printf x > '%s/NewGUI/Screens/mainmenu.uis'",
             directory, directory);
    CHECK(system(command) == 0);

    char asked[1200], real[1200], expected[1200];
    snprintf(asked, sizeof(asked), "%s/newgui/SCREENS/MainMenu.uis", directory);
    snprintf(expected, sizeof(expected), "%s/NewGUI/Screens/mainmenu.uis", directory);
    CHECK(ssr_fs_resolve(asked, real, sizeof(real)));
    CHECK(strcmp(real, expected) == 0);

    FILE *file = ssr_fs_open(asked, "rb");
    CHECK(file != NULL);
    if (file) {
        CHECK(fgetc(file) == 'x');
        fclose(file);
    }

    snprintf(asked, sizeof(asked), "%s/NewGUI/Screens/Missing.uis", directory);
    CHECK(!ssr_fs_resolve(asked, real, sizeof(real)));
    CHECK(ssr_fs_open(asked, "rb") == NULL);
    CHECK(!ssr_fs_resolve(expected, real, 8));
}

int main(void) {
    test_finds_a_name_in_another_case();
    return TEST_RESULT();
}
