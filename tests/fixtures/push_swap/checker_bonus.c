/* Minimal reference checker used by the test-suite. */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

static int a[1024], b[1024], na, nb;

static void rot(int *s, int n, int rev)
{
    int t;

    if (n < 2) return;
    if (!rev) { t = s[0]; memmove(s, s + 1, sizeof(int) * (n - 1)); s[n - 1] = t; }
    else { t = s[n - 1]; memmove(s + 1, s, sizeof(int) * (n - 1)); s[0] = t; }
}

static void sw(int *s, int n)
{
    int t;

    if (n > 1) { t = s[0]; s[0] = s[1]; s[1] = t; }
}

static void push(int *d, int *nd, int *s, int *ns)
{
    if (!*ns) return;
    memmove(d + 1, d, sizeof(int) * (*nd)++);
    d[0] = s[0];
    memmove(s, s + 1, sizeof(int) * --(*ns));
}

static int apply(const char *o)
{
    if (!strcmp(o, "sa")) sw(a, na);
    else if (!strcmp(o, "sb")) sw(b, nb);
    else if (!strcmp(o, "ss")) { sw(a, na); sw(b, nb); }
    else if (!strcmp(o, "pa")) push(a, &na, b, &nb);
    else if (!strcmp(o, "pb")) push(b, &nb, a, &na);
    else if (!strcmp(o, "ra")) rot(a, na, 0);
    else if (!strcmp(o, "rb")) rot(b, nb, 0);
    else if (!strcmp(o, "rr")) { rot(a, na, 0); rot(b, nb, 0); }
    else if (!strcmp(o, "rra")) rot(a, na, 1);
    else if (!strcmp(o, "rrb")) rot(b, nb, 1);
    else if (!strcmp(o, "rrr")) { rot(a, na, 1); rot(b, nb, 1); }
    else return 0;
    return 1;
}

int main(int ac, char **av)
{
    char line[8], c;
    size_t l = 0;
    ssize_t r;

    if (ac < 2) return 0;
    for (int i = 1; i < ac; i++)
    {
        char *end;
        long x = strtol(av[i], &end, 10);

        if (!*av[i] || *end || x < INT_MIN || x > INT_MAX) return (write(2, "Error\n", 6), 1);
        for (int j = 0; j < na; j++) if (a[j] == x) return (write(2, "Error\n", 6), 1);
        a[na++] = (int)x;
    }
    while ((r = read(0, &c, 1)) > 0)
    {
        if (c != '\n') { if (l >= 4) return (write(2, "Error\n", 6), 1); line[l++] = c; continue; }
        line[l] = 0;
        l = 0;
        if (!apply(line)) return (write(2, "Error\n", 6), 1);
    }
    if (l) { line[l] = 0; if (!apply(line)) return (write(2, "Error\n", 6), 1); }
    for (int i = 1; i < na; i++) if (a[i - 1] > a[i]) return (write(1, "KO\n", 3), 0);
    write(1, nb ? "KO\n" : "OK\n", 3);
    return 0;
}
