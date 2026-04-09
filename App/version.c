
#ifdef VERSION_STRING
    #define VER     " "VERSION_STRING
#else
    #define VER     ""
#endif

#ifdef ENABLE_FEAT_F4HWN
    /* Всегда показываем VERSION_STRING_1 (например "v0.22") — не зависит от пресета */
    const char Version[]      = VERSION_STRING_1;
    const char Edition[]      = EDITION_STRING;
#else
    const char Version[]      = AUTHOR_STRING VER;
#endif

const char UART_Version[] = "UV-K5 Firmware, " AUTHOR_STRING VER "\r\n";
