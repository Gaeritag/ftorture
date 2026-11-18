# 🔥 ftorture — testeur de libft (sujet v19.3)

🇬🇧 [English version](README.md)

> *Ta libft va parler.*

**ftorture** fait passer ta libft à la question : chaque fonction tourne dans son
propre processus, face à la **vraie** fonction de la libc. On regarde comment elle
crashe, ce qu'elle fuit et ce qu'elle lit en dehors de sa mémoire.

Inspiré de [Caesarovich/zft](https://github.com/Caesarovich/zft), réécrit en C : rien à
télécharger, il faut juste `cc`, `make` et `nm`. Pensé pour **Linux** (postes 42).

## Installation

```bash
git clone https://github.com/HorusExE69/ftorture.git ~/ftorture
cd ~/ftorture && bash install.sh
source ~/.zshrc        # ou ~/.bashrc
```

`install.sh` ajoute la commande **`torture`** à bash, zsh et fish.

## Utilisation

```bash
torture                       # dans le dossier de ta libft
torture ~/libft               # ou en donnant le chemin
torture -f split,itoa -v      # quelques fonctions, avec tous les détails
torture -p 3                  # seulement la partie 3 (listes)
torture -s                    # mode strict : les ▲ comptent comme erreurs
```

| Option | Effet |
|---|---|
| `-s`, `--speculative` | les erreurs spéculatives (▲) font échouer |
| `-v`, `--verbose` | affiche le détail de chaque échec |
| `-f`, `--func LISTE` | `ft_split,itoa,strlcat` (le préfixe `ft_` est facultatif) |
| `-p`, `--part N` | partie 1, 2 ou 3 |
| `--no-checks` | saute Makefile / norme / nm / prototypes |
| `--no-norm` | saute norminette |
| `--seed N` | graine du fuzzing (42 par défaut) |
| `--timeout S` | timeout par test (3 s par défaut) |
| `--no-color` | sans couleurs |

Un rapport complet sans couleurs est écrit dans `ftorture.log`, dans le dossier de
ftorture. Rien n'est jamais écrit dans ta libft, donc pas de fichier en trop dans ton rendu.

## Ce qui est vérifié

| Bloc | Détail |
|---|---|
| **Makefile** | règles `$(NAME) all clean fclean re`, `cc`, `-Wall -Wextra -Werror` sur chaque fichier (lu avec `make -nB`, même avec des `@`), `ar` et pas libtool, pas de `-std=c99`, **relink**, `clean` / `fclean` |
| **Fichiers** | `libft.h` et `ft_*.c` à la racine, fichiers en trop, `README.md` (1re ligne en italique, sections Description / Instructions / Resources) |
| **Code** | norminette (si installée), **fonctions externes interdites** (tout sauf `malloc`, `free`, `write`, avec le fichier fautif), **variables globales**, fonctions auxiliaires non `static`, les 43 fonctions présentes |
| **Prototypes** | chaque prototype de ton `libft.h` comparé par le compilateur au **man Linux** / sujet v19.3, structure `t_list` exacte, `libft.h` qui compile seul |
| **Tests** | environ 400 tests, plus du fuzzing contre la vraie glibc (ou BSD pour `strlcpy`, `strlcat`, `strnstr`) |

## Les instruments de torture

- **Un processus par test** (`fork`) : un crash n'arrête pas les autres tests.
- **Mémoire gardée** : les chaînes de test sont collées à une page interdite, donc
  la moindre lecture au-delà de la fin crashe (`strlcat`, `strnstr`, `substr`, `memchr`…).
- **malloc instrumenté** (`-Wl,--wrap=malloc,--wrap=free`) :
  - un octet écrit hors d'un bloc : crash
  - utilisation après `free` : crash ; **double free** : signalé
  - **fuites** : tout bloc non libéré à la fin du test
  - taille allouée vérifiée (`strdup`, `itoa`, `split`…)
  - blocs neufs remplis de `0xA5` : un `calloc` qui ne met pas à zéro se voit
  - **protection des malloc** (`M`) : le test est rejoué en faisant échouer le 1er,
    le 2e, le 3e… malloc. Ta fonction doit renvoyer `NULL` sans crash ni fuite
    (indispensable pour `ft_split` et `ft_lstmap`).

## Ta fonction face à la vraie

Pour les entrées tordues (NULL, `n = 0`, dépassements…), ftorture lance **ta**
fonction et **la vraie** dans deux processus séparés, puis compare :

| ta fonction | la vraie | verdict |
|---|---|---|
| crash | crash | ✔ OK : même comportement que l'originale |
| crash | pas de crash | ✘ **erreur**, même sans `-s` (ex : `ft_memcpy(NULL, NULL, 0)`) |
| pas de crash | crash | ▲ **spéculatif** : tu protèges alors que la libc ne le fait pas |
| pas de crash | pas de crash | les résultats doivent être identiques |

Les parties 2 et 3 n'ont pas d'originale dans la libc : la référence est alors une
implémentation simple, sans protection contre NULL.

Les ▲ sont des avertissements : ils ne font pas échouer ftorture, sauf avec `-s`. À toi
de savoir les défendre en correction.

## Légende

| | |
|---|---|
| `✔` | ok |
| `✔` (cyan) | crash identique à la libc (accepté) |
| `✘` | erreur |
| `S` | crash (segfault, bus error, abort) |
| `T` | timeout, sûrement une boucle infinie |
| `L` | fuite mémoire |
| `M` | test de protection des malloc |
| `▲` | spéculatif |

## Ajouter un test

Les tests sont dans `src/t_part1.c`, `src/t_part2.c` et `src/t_part3.c`.

```c
TEST(ft_strlen, "mon test")
{
	EQ_SZ(ft_strlen("abc"), 3);
}

VS(ft_strlen, "comparé à la libc")
{
	OUT("%zu", PICK(ft_strlen, r_strlen)("abc"));
}

MTEST(ft_strdup, "malloc protégé")
{
	ARM();
	char *d = ft_strdup("x");
	DISARM();
	if (FAILING)
		CHECK(!d, "doit renvoyer NULL");
	free(d);
}
```

- **Assertions** : `CHECK`, `EQ_INT`, `EQ_SZ`, `EQ_PTR`, `EQ_STR`, `EQ_MEM`
- **Helpers** : `gstr`, `gmem`, `gbuf` (mémoire gardée), `alloc_size`,
  `cap_open` / `cap_read` (sorties sur un fd), `INPUT` (décrit l'entrée en cas d'échec)
- `VS_SPEC` : comme `VS`, mais une différence de résultat n'est que spéculative

## Licence

GPL-3.0, comme le projet d'origine.
