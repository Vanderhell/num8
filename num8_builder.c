#include "num8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static int trim_eol(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    {
        s[n - 1] = '\0';
        n--;
    }
    return (int)n;
}

static int install_temp_output(const char* temp_path, const char* output_path)
{
#if defined(_WIN32)
    return MoveFileExA(temp_path, output_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temp_path, output_path) == 0;
#endif
}

int main(int argc, char** argv)
{
    FILE* in;
    num8_engine_t engine = {0};
    char temp_out[512];
    char line[256];
    unsigned long long line_no = 0;
    unsigned long long added = 0;
    unsigned long long duplicates = 0;
    int saw_input = 0;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <input.txt> <output.num8>\n", argv[0]);
        return 2;
    }

    in = fopen(argv[1], "rb");
    if (in == NULL)
    {
        fprintf(stderr, "Cannot open input: %s\n", argv[1]);
        return 1;
    }

    if (snprintf(temp_out, sizeof(temp_out), "%s.tmp", argv[2]) < 0)
    {
        fclose(in);
        fprintf(stderr, "Cannot prepare temp output: %s\n", argv[2]);
        return 1;
    }

    remove(temp_out);
    if (num8_create(temp_out, &engine) != NUM8_STATUS_OK)
    {
        fclose(in);
        fprintf(stderr, "Cannot create temp output: %s\n", temp_out);
        return 1;
    }

    while (fgets(line, (int)sizeof(line), in) != NULL)
    {
        num8_status_t st;
        line_no++;
        saw_input = 1;
        trim_eol(line);

        if (line[0] == '\0')
        {
            fprintf(stderr, "Invalid empty line at %llu\n", line_no);
            num8_close(&engine);
            remove(temp_out);
            fclose(in);
            return 1;
        }

        st = num8_add_str(&engine, line);
        if (st == NUM8_STATUS_ADDED)
        {
            added++;
            continue;
        }
        if (st == NUM8_STATUS_ALREADY_EXISTS)
        {
            duplicates++;
            continue;
        }

        fprintf(stderr, "Invalid value at line %llu: '%s' (status=%d)\n", line_no, line, (int)st);
        num8_close(&engine);
        remove(temp_out);
        fclose(in);
        return 1;
    }

    fclose(in);

    if (!saw_input)
    {
        fprintf(stderr, "Input is empty\n");
        num8_close(&engine);
        remove(temp_out);
        return 1;
    }

    if (num8_flush(&engine) != NUM8_STATUS_OK)
    {
        num8_close(&engine);
        remove(temp_out);
        fprintf(stderr, "Flush failed\n");
        return 1;
    }

    if (num8_close(&engine) != NUM8_STATUS_OK)
    {
        remove(temp_out);
        fprintf(stderr, "Close failed\n");
        return 1;
    }

    if (!install_temp_output(temp_out, argv[2]))
    {
        remove(temp_out);
        fprintf(stderr, "Cannot install output: %s\n", argv[2]);
        return 1;
    }
    remove(temp_out);

    fprintf(stdout, "Build done. added=%llu duplicates=%llu\n", added, duplicates);
    return 0;
}
