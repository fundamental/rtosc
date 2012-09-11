bool rtosc_dispatch_complex_pattern(const char *pattern)
{
    do {
        switch(*pattern) {
            case '*':
            case '?':
            case '[':
            case '{':
                return true;
        }
    } while(*++pattern);

    return false;
}

bool rtosc_dispatch_match(const char *path, const char *pattern)
{
    if(!rtosc_dispatch_complex_pattern(*path))
        return !strcmp(path, pattern);
    else //XXX unimplemented
        return false;
}
