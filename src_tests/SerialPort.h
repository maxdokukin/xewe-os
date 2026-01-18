void test() {
    auto banner = [&](string_view fn) {
        printf_raw("[TEST] ------------------------------------------------\r\n");
        printf_raw("[TEST] %.*s BEGIN\r\n", int(fn.size()), fn.data());
    };
    auto done = [&](string_view fn) {
        printf_raw("[TEST] %.*s END\r\n", int(fn.size()), fn.data());
        printf_raw("[TEST] ------------------------------------------------\r\n");
    };

    // RAW OUTPUT
    banner("print_raw");
    printf_raw("[TEST] in : \"raw\"\r\n");
    print_raw("raw");
    printf_raw("[TEST] out: printed\r\n");
    done("print_raw");

    banner("println_raw");
    printf_raw("[TEST] in : \"line\"\r\n");
    println_raw("line");
    printf_raw("[TEST] out: printed with CRLF\r\n");
    done("println_raw");

    banner("printf_raw");
    printf_raw("[TEST] in : fmt=\"num=%%d str=%%s\", 42, \"ok\"\r\n");
    printf_raw("num=%d str=%s\r\n", 42, "ok");
    printf_raw("[TEST] out: printed\r\n");
    done("printf_raw");

    // BOXED PRINT API
    banner("print_separator");
    printf_raw("[TEST] in : total_width=20, fill='-', edge='+'\r\n");
    print_separator(20, '-', '+');
    printf_raw("[TEST] out: printed\r\n");
    done("print_separator");

    banner("print_spacer");
    printf_raw("[TEST] in : total_width=20, edge='|'\r\n");
    print_spacer(20, '|');
    printf_raw("[TEST] out: printed\r\n");
    done("print_spacer");

    banner("print_header");
    printf_raw("[TEST] in : message=\"Header\\sepSub\", total_width=20, edge='|', sep_edge='+', sep_fill='-'\r\n");
    print_header("Header\\sepSub", 20, '|', '+', '-');
    printf_raw("[TEST] out: printed\r\n");
    done("print_header");

    banner("print");
    printf_raw("[TEST] in : message=\"left\", edge='|', align='l', width=10, ml=1, mr=1, end=CRLF\r\n");
    print("left",  '|', 'l', 10, 1, 1, kCRLF);
    printf_raw("[TEST] out: printed\r\n");
    printf_raw("[TEST] in : message=\"center\", edge='|', align='c', width=12, ml=0, mr=0, end=CRLF\r\n");
    print("center",'|', 'c', 12, 0, 0, kCRLF);
    printf_raw("[TEST] out: printed\r\n");
    printf_raw("[TEST] in : message=\"right\", edge='|', align='r', width=12, ml=2, mr=0, end=CRLF\r\n");
    print("right", '|', 'r', 12, 2, 0, kCRLF);
    printf_raw("[TEST] out: printed\r\n");
    done("print");

    banner("printf (boxed)");
    printf_raw("[TEST] in : edge='|', align='l', width=10, ml=0, mr=0, end=CRLF, fmt=\"fmt %%d %%s\", 7, \"seven\"\r\n");
    printf('|','l', 10, 0, 0, kCRLF, "fmt %d %s", 7, "seven");
    printf_raw("[TEST] out: printed\r\n");
    done("printf (boxed)");

    // INPUT AND LINE UTILITIES — no synthetic input
    banner("has_line/read_line");
    printf_raw("[TEST] in : none; expect no line\r\n");
    flush_input();
    bool hl = has_line();
    printf_raw("[TEST] out: has_line=%s\r\n", hl ? "true" : "false");
    string got = read_line();
    printf_raw("[TEST] out: read_line=\"%s\"\r\n", got.c_str());
    printf_raw("[TEST] out: post: has_line=%s\r\n", has_line() ? "true" : "false");
    done("has_line/read_line");

    banner("flush_input");
    printf_raw("[TEST] in : call flush_input()\r\n");
    flush_input();
    printf_raw("[TEST] out: cleared\r\n");
    done("flush_input");

    banner("read_line_with_timeout");
    printf_raw("[TEST] in : timeout_ms=10; expect timeout\r\n");
    string outLine;
    bool ok = read_line_with_timeout(outLine, 10);
    printf_raw("[TEST] out: ok=%s, line=\"%s\"\r\n", ok ? "true" : "false", outLine.c_str());
    done("read_line_with_timeout");

    banner("write_line_crlf");
    printf_raw("[TEST] in : \"EOL test\"\r\n");
    write_line_crlf("EOL test");
    printf_raw("[TEST] out: printed\r\n");
    done("write_line_crlf");

    // GETTERS — default paths only, short timeouts
    banner("get_int");
    printf_raw("[TEST] in : prompt=\"int?\", range=[0..100], retries=1, timeout=0, default=5\r\n");
    flush_input();
    bool succ = false;
    int v = get_int("int?", 0, 100, 1, 0, 5, std::ref(succ));
    printf_raw("[TEST] out: value=%d, success=%s\r\n", v, succ ? "true" : "false");
    done("get_int");

    banner("get_uint8");
    printf_raw("[TEST] in : prompt=\"u8?\", range=[0..255], retries=1, timeout=0, default=9\r\n");
    flush_input();
    succ = false;
    uint8_t v8 = get_uint8("u8?", 0, 255, 1, 0, 9, std::ref(succ));
    printf_raw("[TEST] out: value=%u, success=%s\r\n", static_cast<unsigned>(v8), succ ? "true" : "false");
    done("get_uint8");

    banner("get_uint16");
    printf_raw("[TEST] in : prompt=\"u16?\", range=[0..10000], retries=1, timeout=0, default=1\r\n");
    flush_input();
    succ = false;
    uint16_t v16 = get_uint16("u16?", 0, 10000, 1, 0, 1, std::ref(succ));
    printf_raw("[TEST] out: value=%u, success=%s\r\n", static_cast<unsigned>(v16), succ ? "true" : "false");
    done("get_uint16");

    banner("get_uint32");
    printf_raw("[TEST] in : prompt=\"u32?\", range=[0..1000000], retries=1, timeout=0, default=2\r\n");
    flush_input();
    succ = false;
    uint32_t v32 = get_uint32("u32?", 0u, 1000000u, 1, 0, 2u, std::ref(succ));
    printf_raw("[TEST] out: value=%lu, success=%s\r\n", static_cast<unsigned long>(v32), succ ? "true" : "false");
    done("get_uint32");

    banner("get_float");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    float vf = get_float("float?", -10.5f, 10.5f, 1, 0, 3.14f, std::ref(succ));
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    banner("get_string");
    printf_raw("[TEST] in : prompt=\"str?\", len=[3..10], retries=1, timeout=0, default=\"xx\"\r\n");
    flush_input();
    succ = false;
    string s = get_string("str?", 3, 10, 1, 0, "xx", std::ref(succ));
    printf_raw("[TEST] out: value=\"%s\", success=%s\r\n", s.c_str(), succ ? "true" : "false");
    done("get_string");

    banner("get_yn");
    printf_raw("[TEST] in : prompt=\"yn?\", retries=1, timeout=0, default=false\r\n");
    flush_input();
    succ = false;
    bool b = get_yn("yn?", 1, 0, false, std::ref(succ));
    printf_raw("[TEST] out: value=%s, success=%s\r\n", b ? "true" : "false", succ ? "true" : "false");
    done("get_yn");

    banner("get_float (5 sec timeout)");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    vf = get_float("float?", -10.5f, 10.5f, 1, 5999, 3.14f, std::ref(succ));
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    banner("get_float (inf retries)");
    printf_raw("[TEST] in : prompt=\"float?\", range=[-10.5..10.5], retries=1, timeout=0, default=3.14\r\n");
    flush_input();
    succ = false;
    vf = get_float("float?", -1.5f, 1.5f);
    printf_raw("[TEST] out: value=%g, success=%s\r\n", static_cast<double>(vf), succ ? "true" : "false");
    done("get_float");

    // SUMMARY
    banner("summary");
    printf_raw("[TEST] in : none\r\n");
    print_separator(16, '=', '+');
    print("done", '|', 'c', 10, 0, 0, kCRLF);
    print_separator(16, '=', '+');
    printf_raw("[TEST] out: printed\r\n");
    done("summary");
}