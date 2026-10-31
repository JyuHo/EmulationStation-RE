# EmulationStation-RE

[![Build](https://github.com/JyuHo/EmulationStation-RE/actions/workflows/build.yml/badge.svg)](https://github.com/JyuHo/EmulationStation-RE/actions/workflows/build.yml)

Interface graphique de lancement d'émulateurs, pilotable entièrement à la manette.
EmulationStation-RE est un fork de l'[EmulationStation](https://github.com/Aloshi/EmulationStation)
d'Alec « Aloshi » Lofquist (v2.0.1a, 2015), en cours de **modernisation complète** pour
Linux.

> **État : en développement.** L'application compile et démarre, mais la migration est en
> cours (phase 3 sur 12). Les emplacements des fichiers de configuration changent pendant
> cette phase : lisez la section [Fichiers et emplacements](#fichiers-et-emplacements)
> avant de l'utiliser avec une configuration existante.

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
| 3 | Suppression de Boost, chemins XDG, propriété mémoire | **en cours** (PR #3 à #23) : Boost supprimé, XDG partiel |
| 4 | Eigen → glm | à faire |
| 5 | SDL2 → SDL3 | à faire |
| 6 | Rendu OpenGL 3.3 core | à faire |
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
    sdl2-compat freetype2 eigen curl alsa-lib libglvnd mesa libwebp
# Pour les tests :
sudo pacman -S --needed catch2 xorg-server-xvfb imagemagick
```

stb, pugixml et nanosvg sont fournis dans `external/`. Boost n'est plus nécessaire.

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
| `--vsync <1\|on\|0\|off>` | active ou désactive la synchronisation verticale (activée par défaut) |
| `--gamelist-only` | ne parcourt pas les dossiers de ROMs, lit uniquement les `gamelist.xml` |
| `--ignore-gamelist` | ignore les `gamelist.xml` (dépannage) |
| `--draw-framerate` | affiche le nombre d'images par seconde |
| `--no-exit` | masque l'entrée « Quitter » du menu |
| `--debug` | journal plus détaillé |
| `--scrape` | scraper en ligne de commande (services actuellement hors ligne, voir plus bas) |
| `--help`, `-h` | aide |

Variable d'environnement **`ES_HOME=<dossier>`** : mode portable. Les dossiers gérés par
le module XDG sont alors `<dossier>/{config,data,state,cache}/emulationstation/`
(voir la limite ci-dessous). Une option `--home` équivalente est prévue.

**F4** quitte l'application à tout moment.

## Fichiers et emplacements

Le projet passe de `~/.emulationstation/` aux dossiers XDG. La transition n'est pas
terminée : aujourd'hui les fichiers sont répartis ainsi.

| Fichier | Emplacement actuel |
|---------|--------------------|
| `es_systems.cfg` | `$XDG_CONFIG_HOME/emulationstation/` (par défaut `~/.config/emulationstation/`), puis `$XDG_CONFIG_DIRS/emulationstation/` (par défaut `/etc/xdg/emulationstation/`) |
| `gamelist.xml` | dossier de ROMs du système, puis `$XDG_DATA_HOME/emulationstation/gamelists/<système>/` (par défaut `~/.local/share/...`), puis `$XDG_DATA_DIRS` |
| `es_log.txt` | `$XDG_STATE_HOME/emulationstation/` (par défaut `~/.local/state/emulationstation/`) |
| `es_settings.cfg`, `es_input.cfg` | `~/.emulationstation/` *(passage en XDG prévu)* |
| `es_theme_default.xml`, `themes/` | `~/.emulationstation/` et `/etc/emulationstation/themes/` *(passage en XDG prévu)* |
| images téléchargées par le scraper | `~/.emulationstation/downloaded_images/` *(passage en XDG prévu)* |

> **Vous aviez déjà `~/.emulationstation/es_systems.cfg` ?** La migration automatique
> n'est pas encore active. Copiez le fichier vous-même :
> `mkdir -p ~/.config/emulationstation && cp ~/.emulationstation/es_systems.cfg ~/.config/emulationstation/`.
> Sinon ES écrit un fichier d'exemple au nouvel emplacement et s'arrête.

À terme (phase 3.E), tout passera par les dossiers XDG, `~/.emulationstation/` sera copié
(jamais déplacé ni supprimé) au premier lancement, et `ES_HOME` couvrira tous les fichiers.

## Configurer les systèmes (`es_systems.cfg`)

`es_systems.cfg` (XML) décrit vos systèmes : nom, dossier des ROMs, extensions et
commande de lancement. Au premier lancement sans ce fichier, ES écrit un exemple commenté
dans `~/.config/emulationstation/es_systems.cfg` puis s'arrête. **Les émulateurs
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

Au premier lancement, ES demande de configurer un périphérique :

1. maintenez un bouton du périphérique à configurer (le clavier compte aussi) ;
2. appuyez sur les boutons demandés ; maintenir un bouton quelques secondes passe une entrée facultative ;
3. vérifiez avec haut/bas, corrigez avec A, puis choisissez « SAVE ».

La configuration est enregistrée dans `es_input.cfg`. On peut la refaire à tout moment
depuis le menu (Start → « CONFIGURE INPUT »). Si une manette ne répond plus, supprimez
`es_input.cfg` pour relancer l'assistant. La reconnaissance automatique des manettes par
SDL3 arrive en phase 5 ; l'assistant restera disponible en repli.

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
`basic`, `detailed`, `grid`), cherchés dans `~/.emulationstation/themes/` et
`/etc/emulationstation/themes/`. Sans thème, l'interface est minimale.
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
publiée en artefact).

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
