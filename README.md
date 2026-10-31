# EmulationStation-RE

[![Build](https://github.com/JyuHo/EmulationStation-RE/actions/workflows/build.yml/badge.svg)](https://github.com/JyuHo/EmulationStation-RE/actions/workflows/build.yml)

Interface graphique de lancement d'émulateurs, pilotable entièrement à la manette.
EmulationStation-RE est un fork de l'[EmulationStation](https://github.com/Aloshi/EmulationStation)
d'Alec « Aloshi » Lofquist (v2.0.1a, 2015), en cours de **modernisation complète** pour
Linux.

> **État : en développement.** L'application compile et démarre, mais la migration est en
> cours (phase 4 sur 12). Les fichiers sont désormais rangés dans les dossiers XDG ; une
> configuration existante dans `~/.emulationstation/` est copiée au premier lancement
> (voir [Fichiers et emplacements](#fichiers-et-emplacements)).

## Sommaire

- [Objectifs](#objectifs)
- [Avancement](#avancement)
- [Compilation](#compilation)
- [Lancement](#lancement)
- [Fichiers et emplacements](#fichiers-et-emplacements)
- [Configurer les systèmes](#configurer-les-systèmes-es_systemscfg)
- [Manettes et clavier](#manettes-et-clavier)
- [Listes de jeux et scraping](#listes-de-jeux-et-scraping)
- [Thèmes](#thèmes)
- [Tests et qualité](#tests-et-qualité)
- [Contribuer](#contribuer)
- [Licence et crédits](#licence-et-crédits)

## Objectifs

| Domaine | Cible |
|---------|-------|
| Plateforme | Linux x86_64 uniquement, **Arch Linux / CachyOS** |
| Langage | **C++23**, sans avertissement avec GCC 14+ et Clang 18+ |
| Build | CMake ≥ 3.25, Ninja, presets |
| Fenêtre, entrées, audio | **SDL3** (manettes via `SDL_Gamepad`) |
| Rendu | **OpenGL 3.3 core** à shaders |
| Images | **stb_image** + **libwebp** (+ nanosvg pour le SVG) |
| Vidéo et musique | **FFmpeg** |
| JSON | **nlohmann/json**, réservé au scraping (les fichiers XML existants restent en XML) |
| Thèmes | modèle **Recalbox 10**, compatibilité avec le format Aloshi v3 |
| Traductions | catalogues **gettext** `.pot` / `.po` / `.mo` |
| Fichiers | spécification **XDG Base Directory**, aucun chemin en dur |

Raspberry Pi, Windows, macOS et OpenGL ES ne sont plus pris en charge.

## Avancement

| Phase | Sujet | État |
|------:|-------|------|
| 1 | CMake moderne, C++23, CI Arch, FreeImage → stb_image + libwebp | fait (PR #1) |
| 2 | Zéro avertissement, `-Werror`, clang-format/tidy, tests Catch2, test de fumée, journal | fait (PR #2) |
| 3 | Suppression de Boost, chemins XDG, propriété mémoire | fait (PR #3 à #28) |
| 4 | Eigen → glm | fait (PR #30 à #32) |
| 5 | SDL2 → SDL3, manettes reconnues par `SDL_Gamepad` | fait (PR #33, #34, #37) |
| 6 | Rendu OpenGL 3.3 core | **en cours** : moteur à shaders et regroupement faits (PR #40, #42, #44, #45) ; mesure sur iGPU à faire |
| 7 | Images animées, chargement asynchrone, cache | à faire |
| 8 | Audio (sons, musique de fond) | à faire |
| 9 | Vidéo avec FFmpeg | à faire |
| 10 | Traductions gettext | à faire |
| 11 | Thèmes Recalbox 10 | à faire |
| 12 | Paquet Arch, installation, session kiosque | à faire |

Détails : [plan de modernisation](docs/MODERNIZATION_PLAN.md) et
[dernier audit](docs/AUDIT.md).

## Compilation

### Dépendances (Arch Linux / CachyOS)

```bash
sudo pacman -S --needed base-devel cmake ninja clang \
    sdl3 freetype2 fontconfig glm curl alsa-lib libglvnd mesa libwebp
# Pour les tests :
sudo pacman -S --needed catch2 xorg-server-xvfb imagemagick xdotool
```

stb, pugixml, nanosvg et glad (chargeur OpenGL) sont fournis dans `external/`. Boost, Eigen et SDL2 ne sont plus nécessaires.

### Avec les presets (recommandé)

```bash
cmake --preset release
cmake --build --preset release
./build/release/emulationstation --windowed --resolution 1280 720
```

| Preset | Usage |
|--------|-------|
| `debug` | développement : tests activés, avertissements bloquants (`-Werror`) |
| `release` | binaire optimisé |
| `relwithdebinfo` | optimisé avec symboles, tests activés (la CI y ajoute `-Werror`) |
| `asan` | AddressSanitizer + UndefinedBehaviorSanitizer, tests activés |
| `clang-debug` | comme `debug`, compilé avec Clang |

### Sans presets

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Les builds dans l'arbre source (`cmake .`) sont refusés.

### Options CMake

| Option | Défaut | Effet |
|--------|--------|-------|
| `ES_BUILD_TESTS` | `OFF` | compile les tests unitaires (Catch2 v3 requis) |
| `ES_WARNINGS_AS_ERRORS` | `OFF` | transforme les avertissements en erreurs |
| `ES_ENABLE_SANITIZERS` | `OFF` | active ASan et UBSan |
| `ES_ENABLE_LTO` | `OFF` | optimisation à l'édition de liens (builds Release) |

`cmake --install build` installe pour l'instant uniquement le binaire ; l'installation
des ressources et le paquet Arch arrivent en phase 12.

## Lancement

```bash
emulationstation [options]
```

| Option | Effet |
|--------|-------|
| `--resolution <largeur> <hauteur>` | force une résolution |
| `--windowed` | fenêtre au lieu du plein écran (à combiner avec `--resolution`) |
| `--home <dossier>` | mode portable (voir ci-dessous) |
| `--vsync <1\|on\|0\|off>` | active ou désactive la synchronisation verticale (activée par défaut) |
| `--gamelist-only` | ne parcourt pas les dossiers de ROMs, lit uniquement les `gamelist.xml` |
| `--ignore-gamelist` | ignore les `gamelist.xml` (dépannage) |
| `--draw-framerate` | affiche le nombre d'images par seconde |
| `--no-exit` | masque l'entrée « Quitter » du menu |
| `--debug` | journal plus détaillé |
| `--scrape` | scraper en ligne de commande (services actuellement hors ligne, voir plus bas) |
| `--help`, `-h` | aide |

Rendu : **OpenGL 3.3 core** est requis (Mesa le fournit pour Intel, AMD et llvmpipe ; pas de
repli en profil compatibilité). La version obtenue est écrite dans le journal
(`GL_VERSION`, `GL_RENDERER`). **`ES_GL_DEBUG=1`** active la sortie de débogage OpenGL
(extension KHR_debug) dans une version optimisée : chaque erreur GL devient une ligne
`[ERROR]` du journal (toujours active dans une version Debug).

Mode portable : **`--home <dossier>`** ou la variable **`ES_HOME=<dossier>`** (l'option
l'emporte) place tous les fichiers dans `<dossier>/{config,data,state,cache}/emulationstation/`.
Dans ce mode, `~` dans `es_systems.cfg` désigne toujours le vrai dossier personnel et
`~/.emulationstation/` n'est pas copié.

**F4** quitte l'application à tout moment.

## Fichiers et emplacements

EmulationStation-RE suit la spécification
[XDG Base Directory](https://specifications.freedesktop.org/basedir-spec/latest/).
Les variables `XDG_*` vides ou relatives sont ignorées.

| Contenu | Dossier utilisateur (lecture et écriture) | Dossiers système (lecture seule, en repli) |
|---------|-------------------------------------------|--------------------------------------------|
| `es_systems.cfg`, `es_settings.cfg`, `es_input.cfg`, `gamecontrollerdb.txt`, `es_theme_default.xml` | `$XDG_CONFIG_HOME/emulationstation/` (défaut `~/.config/emulationstation/`) | `$XDG_CONFIG_DIRS/emulationstation/` (défaut `/etc/xdg/emulationstation/`), puis `<sysconfdir>/xdg/emulationstation/` |
| `themes/`, `gamelists/<système>/gamelist.xml`, `downloaded_images/` | `$XDG_DATA_HOME/emulationstation/` (défaut `~/.local/share/emulationstation/`) | `$XDG_DATA_DIRS/emulationstation/` (défaut `/usr/local/share` puis `/usr/share`), puis `<datadir>/emulationstation/`, puis `<dossier du binaire>/../share/emulationstation/` |
| `es_log.txt` | `$XDG_STATE_HOME/emulationstation/` (défaut `~/.local/state/emulationstation/`) | — |
| caches (à venir) | `$XDG_CACHE_HOME/emulationstation/` (défaut `~/.cache/emulationstation/`) | — |

Les dossiers utilisateur sont créés avec les droits `0700`. Une `gamelist.xml` placée dans
le dossier de ROMs du système reste prioritaire, comme avant. Un thème du dossier
utilisateur remplace un thème système de même nom.

**Migration depuis `~/.emulationstation/`** : au premier lancement, les fichiers
`es_systems.cfg`, `es_settings.cfg`, `es_input.cfg`, `es_theme_default.xml` et les
dossiers `themes/`, `gamelists/`, `downloaded_images/` sont **copiés** vers les dossiers
ci-dessus. Rien n'est déplacé, supprimé ni écrasé : un fichier déjà présent côté XDG est
conservé. Chaque copie est notée dans le journal, et le fichier
`~/.emulationstation/MIGRATED_TO_XDG.txt` indique où sont passés les fichiers ; tant
qu'il existe, la migration ne se refait pas. L'ancien dossier peut être gardé : les
`gamelist.xml` existantes peuvent encore pointer vers des images qui s'y trouvent.

Les anciens emplacements système `/etc/emulationstation/` ne sont plus lus : placez
`es_systems.cfg` et les thèmes système dans `/etc/xdg/emulationstation/` et
`/usr/share/emulationstation/themes/`.

Les polices de repli pour le japonais, le chinois et le coréen sont trouvées par
**fontconfig** (installez par exemple `noto-fonts-cjk`).

## Configurer les systèmes (`es_systems.cfg`)

`es_systems.cfg` (XML) décrit vos systèmes : nom, dossier des ROMs, extensions et
commande de lancement. Au premier lancement sans ce fichier, ES écrit un exemple commenté
dans `~/.config/emulationstation/es_systems.cfg` puis s'arrête (sauf si un ancien
`~/.emulationstation/es_systems.cfg` a été copié). **Les émulateurs
s'installent et se configurent séparément.**

```xml
<systemList>
	<system>
		<!-- Nom court, utilisé en interne et pour choisir le thème. -->
		<name>snes</name>
		<!-- Nom affiché (facultatif). -->
		<fullname>Super Nintendo Entertainment System</fullname>
		<!-- Dossier des ROMs, sous-dossiers compris. « ~ » est remplacé par $HOME. -->
		<path>~/roms/snes</path>
		<!-- Extensions séparées par des espaces, point inclus, sensibles à la casse. -->
		<extension>.smc .sfc .SMC .SFC</extension>
		<!-- Commande shell lancée pour un jeu. -->
		<command>retroarch -L /usr/lib/libretro/snes9x_libretro.so %ROM%</command>
		<!-- Plateforme(s) pour le scraping (facultatif, voir es-app/src/PlatformId.cpp). -->
		<platform>snes</platform>
		<!-- Thème à utiliser (facultatif, <name> par défaut). -->
		<theme>snes</theme>
	</system>
</systemList>
```

Remplacements disponibles dans `<command>` :

| Balise | Remplacée par |
|--------|---------------|
| `%ROM%` | chemin absolu de la ROM, caractères spéciaux du shell échappés |
| `%ROM_RAW%` | chemin absolu de la ROM, non échappé (à mettre entre guillemets) |
| `%BASENAME%` | nom de la ROM sans dossier ni extension (`/foo/bar.rom` → `bar`) |

Les systèmes apparaissent dans l'ordre du fichier. Un système sans aucun jeu dans son
dossier est ignoré ; si aucun système n'est valide, ES affiche une erreur et s'arrête.

## Manettes et clavier

Les manettes que SDL3 reconnaît (Xbox, DualSense, 8BitDo et la plupart des modèles
courants, base intégrée à SDL) sont utilisables dès qu'elles sont branchées, sans
configuration : croix directionnelle, A = bouton du bas, B = bouton de droite, Start,
Select (Back), gâchettes L/R pour page précédente/suivante. Pour le placement Nintendo
(A à droite), mettez `<bool name="GamepadSwapAB" value="true" />` dans `es_settings.cfg`.
Une manette absente de la base de SDL se décrit dans un fichier `gamecontrollerdb.txt`
(format de [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)) placé
dans le dossier de configuration (`~/.config/emulationstation/`), ou dans
`/etc/xdg/emulationstation/` pour tout le système.

Les autres périphériques, et le clavier, se configurent au premier lancement :

1. maintenez un bouton du périphérique à configurer (le clavier compte aussi) ;
2. appuyez sur les boutons demandés ; maintenir un bouton quelques secondes passe une entrée facultative ;
3. vérifiez avec haut/bas, corrigez avec A, puis choisissez « SAVE ».

La configuration est enregistrée dans `es_input.cfg`. On peut la refaire à tout moment
depuis le menu (Start → « CONFIGURE INPUT »), y compris pour une manette reconnue : la
configuration enregistrée dans `es_input.cfg` passe avant celle par défaut. Si une manette
ne répond plus, supprimez son entrée de `es_input.cfg` (ou le fichier) pour revenir à la
configuration par défaut ou relancer l'assistant.

## Listes de jeux et scraping

`gamelist.xml` (XML) contient les métadonnées des jeux : nom, description, image, date,
note, etc. Dès qu'un jeu d'un système a une image, ES utilise la vue détaillée.
Les métadonnées s'éditent dans ES : sélectionnez un jeu, appuyez sur Select, puis
« EDIT THIS GAME'S METADATA ». Les fichiers existants restent lisibles et une réécriture
sans modification produit un fichier identique.

Les scrapers d'origine (TheGamesDB legacy et TheArchive) interrogent des services
**fermés** : le scraping ne fonctionne pas actuellement. Un scraper ScreenScraper, avec
analyse JSON par nlohmann/json, est prévu.

## Thèmes

Le moteur actuel lit les thèmes au format Aloshi v3 (`formatVersion` 3, vues `system`,
`basic`, `detailed`, `grid`), cherchés dans les dossiers `themes/` décrits dans
[Fichiers et emplacements](#fichiers-et-emplacements) (par exemple
`~/.local/share/emulationstation/themes/<nom du thème>/`). Sans thème, l'interface est
minimale.
Le format des thèmes Recalbox 10 (options de thème, variables, vidéo, menus thémés)
arrive en phase 11, avec sa documentation dans `docs/THEMES.md`.

## Tests et qualité

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug                                   # tests unitaires Catch2
tests/smoke/run.sh build/debug/emulationstation 6 /tmp/capture.png   # test de fumée sous Xvfb
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
cmake --build --preset debug --target es-tidy          # clang-tidy (si run-clang-tidy est installé)
```

La CI GitHub Actions compile avec GCC et Clang dans un conteneur `archlinux:base-devel`,
avec `-Werror`, puis lance les tests unitaires et le test de fumée (la capture d'écran est
publiée en artefact). Un troisième job refait la même chose avec AddressSanitizer et
UBSan. Avant la compilation, la CI refuse les chemins écrits en dur et tout code propre à
Windows, macOS, Raspberry Pi ou OpenGL ES, ainsi que tout appel ou type OpenGL hors de
`es-core/src/renderer/`.

Le code suit `.clang-format` ; ne formatez que les lignes modifiées
(`git clang-format origin/main`). Pour que `git blame` ignore le commit de reformatage :

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## Contribuer

Avant toute contribution, humaine ou par un agent, lisez dans l'ordre :

1. [`PROMPT.md`](PROMPT.md) : règles, conventions et point de reprise (section 0) ;
2. [`docs/MODERNIZATION_PLAN.md`](docs/MODERNIZATION_PLAN.md) : tâches et critères de chaque phase ;
3. [`docs/AUDIT.md`](docs/AUDIT.md) : état réel du dépôt au dernier audit.

En bref : une branche `modernize/phase-<n><lettre>-<sujet>` et une PR en français par
sous-objectif, CI verte avec GCC et Clang, aucun chemin en dur, aucune dépendance hors des
dépôts officiels Arch, rien poussé directement sur `main`, et fusion par **merge commit**
(jamais « squash »).

## Licence et crédits

Licence MIT, voir [`LICENSE.md`](LICENSE.md). Code d'origine © 2014 Alec Lofquist.

Bibliothèques fournies dans `external/` : [stb](https://github.com/nothings/stb)
(domaine public / MIT), [pugixml](https://pugixml.org/) (MIT),
[nanosvg](https://github.com/memononen/nanosvg) (zlib).
Recalbox et Batocera servent de références fonctionnelles pour les thèmes et les
traductions ; leur code n'est pas repris.
