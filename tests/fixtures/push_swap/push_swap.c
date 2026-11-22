/* Minimal reference push_swap used by the test-suite (not norm compliant). */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include "push_swap.h"

static int a[1024], b[1024], na, nb, count[11];
static const char *names[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb", "rr", "rra", "rrb", "rrr"};
static char out[1 << 22];
static size_t outlen;

static void op(int k)
{
    int t;

    if (k == 0 && na > 1) { t = a[0]; a[0] = a[1]; a[1] = t; }
    if (k == 3 && nb) { memmove(a + 1, a, sizeof(int) * na++); a[0] = b[0]; memmove(b, b + 1, sizeof(int) * --nb); }
    if (k == 4 && na) { memmove(b + 1, b, sizeof(int) * nb++); b[0] = a[0]; memmove(a, a + 1, sizeof(int) * --na); }
    if (k == 5 && na > 1) { t = a[0]; memmove(a, a + 1, sizeof(int) * (na - 1)); a[na - 1] = t; }
    if (k == 8 && na > 1) { t = a[na - 1]; memmove(a + 1, a, sizeof(int) * (na - 1)); a[0] = t; }
    count[k]++;
    outlen += (size_t)sprintf(out + outlen, "%s\n", names[k]);
}

static int sorted(void)
{
    for (int i = 1; i < na; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

static void selection(void)
{
    while (na > 0)
    {
        int m = 0;
        for (int i = 1; i < na; i++) if (a[i] < a[m]) m = i;
        if (m <= na / 2) while (m--) op(5);
        else for (m = na - m; m--;) op(8);
        op(4);
    }
    while (nb) op(3);
}

static void radix(void)
{
    int rank[1024], n = na;

    for (int i = 0; i < n; i++)
    {
        rank[i] = 0;
        for (int j = 0; j < n; j++) rank[i] += a[j] < a[i];
    }
    memcpy(a, rank, sizeof(int) * n);
    for (int bit = 0; !sorted(); bit++)
    {
        for (int i = 0; i < n; i++)
            if ((a[0] >> bit) & 1) op(5); else op(4);
        while (nb) op(3);
    }
}

static int parse(const char *s, int *v)
{
    char *end;
    long x;

    if (!*s || ((*s == '-' || *s == '+') && !s[1])) return 0;
    x = strtol(s, &end, 10);
    if (*end || x < INT_MIN || x > INT_MAX || (s[0] == ' ')) return 0;
    *v = (int)x;
    return 1;
}

int main(int ac, char **av)
{
    int i = 1, strategy = 3, bench = 0, chosen = 0;
    long bad = 0, pairs = 0;
    static const char *labels[] = {"Simple O(n^2)", "Medium O(n sqrt n)", "Complex O(n log n)", "Adaptive O(n log n)"};

    if (ac < 2) return 0;
    for (; i < ac && !strncmp(av[i], "--", 2); i++)
    {
        const char *f[] = {"--simple", "--medium", "--complex", "--adaptive"};
        int k;

        if (!strcmp(av[i], "--bench")) { bench = 1; continue; }
        for (k = 0; k < 4 && strcmp(av[i], f[k]); k++) ;
        if (k == 4 || chosen) return (write(2, "Error\n", 6), 1);
        strategy = k;
        chosen = 1;
    }
    if (i == ac) return (write(2, "Error\n", 6), 1);
    for (; i < ac; i++)
    {
        if (!parse(av[i], &a[na])) return (write(2, "Error\n", 6), 1);
        for (int j = 0; j < na; j++) if (a[j] == a[na]) return (write(2, "Error\n", 6), 1);
        na++;
    }
    for (int x = 0; x < na; x++) for (int y = x + 1; y < na; y++) { pairs++; bad += a[x] > a[y]; }
    if (!sorted())
    {
        if (strategy == 0) selection();
        else if (strategy == 1 && na > 1) { op(0); op(0); radix(); }
        else radix();
    }
    BREAK_HERE
    write(1, out, outlen);
    if (bench)
    {
        int total = 0;
        for (int k = 0; k < 11; k++) total += count[k];
        fprintf(stderr, "[bench] disorder: %.2f%%\n[bench] strategy: %s\n[bench] total_ops: %d\n[bench]",
                pairs ? bad * 100.0 / pairs : 0.0, labels[strategy], total);
        for (int k = 0; k < 11; k++) fprintf(stderr, " %s: %d", names[k], count[k]);
        fprintf(stderr, "\n");
    }
    return 0;
}
