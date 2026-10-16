/*
 * Host-side check of the Updater's NEW extraction strategy.
 *
 * The old strategy used pgeZipExtractCounted, which walks the archive and writes through newlib stdio;
 * that is what killed the console. The new one reads every entry BY NAME with pgeZipFileRead and writes
 * it with a plain write(2). This mirrors that logic exactly -- parse manifest.txt, then for each path in
 * it read by name and write -- so the premise can be checked here, where a bug is a core dump rather
 * than a powered-off handheld.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <Aurora/Utils/pgeZip.h>

/* The manifest's `file|path|size|crc` lines, as Updater.cpp parses them. */
#define MAX_ENTRIES 512
static char g_path[MAX_ENTRIES][256];
static unsigned int g_size[MAX_ENTRIES];
static int g_count = 0;

static void parse_manifest(const char *text)
{
    const char *line = text;
    while (line && *line && g_count < MAX_ENTRIES)
    {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? (size_t)(eol - line) : strlen(line);
        if (len > 3 && strncmp(line, "file|", 5) == 0)
        {
            char tmp[512];
            if (len < sizeof(tmp))
            {
                memcpy(tmp, line, len); tmp[len] = 0;
                /* file|<path>|<size>|<crc> */
                char *save = NULL;
                char *kind = strtok_r(tmp, "|", &save);
                char *path = strtok_r(NULL, "|", &save);
                char *size = strtok_r(NULL, "|", &save);
                if (kind && path && size)
                {
                    snprintf(g_path[g_count], sizeof(g_path[0]), "%s", path);
                    g_size[g_count] = (unsigned int)strtoul(size, NULL, 10);
                    g_count++;
                }
            }
        }
        line = eol ? eol + 1 : NULL;
    }
}

static void make_parents(const char *path)
{
    char scratch[256];
    snprintf(scratch, sizeof(scratch), "%s", path);
    for (char *p = scratch; *p; p++)
    {
        if (*p != '/') continue;
        *p = 0;
        mkdir(scratch, 0777);
        *p = '/';
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: %s <package.zip>\n", argv[0]); return 2; }

    pgeZip *zip = pgeZipOpen(argv[1]);
    if (!zip) { printf("pgeZipOpen failed\n"); return 1; }

    printf("=== read manifest.txt by name (pgeZipFileRead) ===\n");
    pgeZipFile *m = pgeZipFileRead(zip, "manifest.txt", 0);
    if (!m) { printf("  FAILED\n"); return 1; }
    printf("  %d bytes\n", m->size);
    parse_manifest((const char *)m->data);
    pgeZipFileFree(m);
    printf("  manifest lists %d file(s)\n", g_count);

    printf("=== read each entry BY NAME and write it (the new path) ===\n");
    int ok = 0, failed = 0;
    for (int i = 0; i < g_count; i++)
    {
        printf("  [%d/%d] '%s' (expect %u bytes)\n", i + 1, g_count, g_path[i], g_size[i]);
        pgeZipFile *f = pgeZipFileRead(zip, g_path[i], 0);
        if (!f) { printf("    read FAILED\n"); failed++; continue; }
        if ((unsigned int)f->size != g_size[i])
        { printf("    size MISMATCH: got %d\n", f->size); pgeZipFileFree(f); failed++; continue; }

        make_parents(g_path[i]);
        int fd = open(g_path[i], O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd < 0) { printf("    open FAILED\n"); pgeZipFileFree(f); failed++; continue; }

        int wrote = 0, remaining = f->size;
        while (remaining > 0)
        {
            ssize_t put = write(fd, f->data + wrote, (size_t)remaining);
            if (put <= 0) break;
            wrote += (int)put; remaining -= (int)put;
        }
        close(fd);
        pgeZipFileFree(f);

        if (remaining != 0) { printf("    short write: %d of %u\n", wrote, g_size[i]); failed++; continue; }
        printf("    wrote %d bytes OK\n", wrote);
        ok++;
    }

    pgeZipClose(zip);
    printf("=== result: %d ok, %d failed ===\n", ok, failed);
    return failed == 0 && ok == g_count ? 0 : 1;
}
