# Test data

`bible-kjv.txt.gz`: the King James Version of the Bible (public domain), from
Project Gutenberg eBook #10 (gutenberg.pglaf.org/1/10/10-0.txt). Only the text
is kept: the Project Gutenberg header and footer are removed and line endings
are `\n`. 4 336 576 bytes, 99 594 lines, at most 79 characters per line.

The get_next_line suite decompresses it into `build/` and reads it as one big,
real text file (see "la Bible" in `src/gnl/t_gnl.c`).
