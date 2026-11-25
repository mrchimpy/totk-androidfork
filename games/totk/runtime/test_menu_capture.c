/* Exercise the Android 1.4.3 installer with stand-in words, including the
 * original installation order of the benchmark clock and thermal frame pacing. */
#define TOTK_MENU_CAPTURE_TEST 1
#define TOTK_THERMAL_PATCH_TEST 1
#define main fps60_fixture_main
#include "test_v143_fps60_patch_table.c"
#undef main

static void check_skip(const char *reason) {
    memcpy(pristine, image, IMAGE_SIZE);
    CHECK(run(patch_menu_capture), "menu fix stopped startup on %s", reason);
    CHECK(!memcmp(image, pristine, IMAGE_SIZE), "menu fix wrote bytes on %s", reason);
}

int main(void) {
    const uint64_t site = 0x105bbcc, resume = site + 4;
    const uint64_t tail = FILE_SIZE - 0x1000, stub = tail + 144;
    image = calloc(1, IMAGE_SIZE);
    pristine = malloc(IMAGE_SIZE);
    if (!image || !pristine) return 2;
    module = (Module){
        .name = "main", .digest = "0000000000000000000000000000000000000000000000000000000000000000",
        .base = image, .text_end = TEXT_END, .key_off = FILE_SIZE - 0x8000,
        .patch_off = FILE_SIZE - 0x4000, .file_size = tail - 4, .size = IMAGE_SIZE,
    };
    const Module valid = module;
    put_word(image, site, code_guard_standin(site));
    put_word(image, resume, code_guard_standin(resume));
    memset(image + tail, 0x5a, 128);  /* reserved grass observer space */
    for (unsigned i = 0; i < sizeof bench_time_sites / sizeof *bench_time_sites; i++)
        put_word(image, bench_time_sites[i], code_guard_standin(bench_time_sites[i]));
    for (unsigned i = 0; i < sizeof fps60 / sizeof *fps60; i++)
        put_word(image, fps60[i].off, fps60[i].replacement);

    set_env("TOTK_MENU_CAPTURE_FIX", "0");
    check_skip("opt-out");
    set_env("TOTK_MENU_CAPTURE_FIX", NULL);
    for (uint64_t off = site; off <= resume; off += 4) {
        image[off] ^= 1;
        check_skip("instruction mismatch");
        image[off] ^= 1;
    }
    image[stub + 15] = 1;
    check_skip("occupied veneer");
    image[stub + 15] = 0;
    module.text_end = resume;
    check_skip("truncated instruction pair");
    module = valid;
    module.file_size = FILE_SIZE;
    check_skip("no executable tail padding");
    module = valid;
    module.size = stub + 8;
    check_skip("truncated mapping");
    module = valid;

    set_env("TOTK_BENCH_TIME", "12:34");
    CHECK(run(patch_bench_time), "benchmark clock failed to install");
    memcpy(pristine, image, IMAGE_SIZE);
    CHECK(run(patch_menu_capture), "menu fix failed to install by default");
    CHECK(branch_target(site, word_at(site)) == stub, "menu draw missed the reserved veneer");
    CHECK(word_at(stub) == code_guard_standin(site), "armed load was not replayed");
    CHECK(branch_target(stub + 12, word_at(stub + 12)) == resume, "menu draw resumed at wrong instruction");
    uint8_t code[16];
    memcpy(code, image + stub, sizeof code);
    put_word(image, site, code_guard_standin(site));
    memset(image + stub, 0, sizeof code);
    CHECK(!memcmp(image, pristine, IMAGE_SIZE), "menu fix changed bytes outside its two writes");

    CHECK(run(patch_menu_capture), "menu fix failed before thermal setup");
    set_env("TOTK_THERMAL_STEP", "1");
    CHECK(run(patch_thermal_step), "menu fix collided with thermal frame pacing");
    CHECK(word_at(DATA + 144) == 1, "thermal floor was not initialized");
    CHECK(branch_target(site, word_at(site)) == stub && !memcmp(image + stub, code, sizeof code),
          "thermal setup changed the menu fix");
    check_skip("repeat installation");

    printf("{\"stub\":%llu,\"resume\":%llu,\"code\":\"", (unsigned long long)stub, (unsigned long long)resume);
    for (unsigned i = 0; i < sizeof code; i++) printf("%02x", code[i]);
    puts("\"}");
    free(pristine);
    free(image);
    return failures ? 1 : 0;
}
