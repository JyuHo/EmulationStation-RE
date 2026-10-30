# Plan de modernisation d'EmulationStation-RE

> Document de référence. Il décrit l'état de départ, les choix techniques, et chaque
> phase de travail avec ses tâches, les fichiers concernés, ses critères d'acceptation
> et ses risques. Les phases sont ordonnées : chacune suppose les précédentes terminées.
> Un agent (humain ou IA) qui reprend le travail doit lire aussi [`PROMPT.md`](../PROMPT.md).

---

## 0. Résumé

| Phase | Sujet | Dépend de | Taille estimée |
|------:|-------|-----------|----------------|
| 1 | Build moderne : CMake 3.25+, C++23, presets, CI Arch ; FreeImage → stb_image + libwebp | — | S (fait, PR #1) |
| 2 | Hygiène C++23 et outillage (clang-format, clang-tidy, tests, logs) | 1 | M |
| 3 | Suppression de Boost + module de chemins **XDG** (std::filesystem, std::chrono, std::variant) | 2 | M |
| 4 | Remplacement d'Eigen par une petite bibliothèque mathématique (glm) | 2 | M |
| 5 | Migration SDL2 → SDL3 (fenêtre, événements, manettes, audio) | 3, 4 | L |
| 6 | Nouveau moteur de rendu OpenGL 3.3 core à shaders | 5 | L |
| 7 | Images : WebP/GIF animés, chargement asynchrone, cache VRAM | 6 | M |
| 8 | Audio : sons de navigation et musique de fond sur SDL3 | 5 | M |
| 9 | Vidéo : lecture des « snaps » avec FFmpeg | 6, 8 | L |
| 10 | Internationalisation : gettext .pot/.po/.mo | 3 | M |
| 11 | Thèmes sur le modèle de Recalbox 10 | 6, 7, 9, 10 | XL |
| 12 | Paquet Arch (PKGBUILD), installation, session kiosque | 1 à 11 | S |

Cible unique : **Linux x86_64, Arch Linux / CachyOS**, GCC 14+ ou Clang 18+, Mesa
(OpenGL 3.3 core minimum). Les chemins Raspberry Pi, Mali, Windows et MSVC sont abandonnés.

---

## 1. Audit de l'état de départ (commit `c617097`)

### 1.1 Chiffres

- ~73 000 lignes C++ dont ~42 000 générées (`data/converted/*.cpp`, ressources embarquées)
  et ~30 000 pour la seule table `es-app/src/MameNameMap.cpp`.
- Code « réel » : ~11 000 lignes réparties entre `es-core` (bibliothèque statique : rendu,
  entrées, composants, ressources, thèmes) et `es-app` (exécutable : systèmes, gamelists,
  vues, menus, scrapers).
- Version annoncée : 2.0.1a (Aloshi, 2014-2015). Standard d'origine : C++11.

### 1.2 Dépendances d'origine

| Dépendance | Usage | Ampleur | Sort prévu |
|------------|-------|---------|------------|
| SDL2 | fenêtre, contexte GL, événements, joysticks (API `SDL_Joystick`, pas `SDL_GameController`), audio (`SDL_OpenAudio` + `SDL_MixAudio`) | 16 fichiers, ~146 appels | SDL3 (phase 5) |
| OpenGL 1.x « fixed function » | `glVertexPointer`, `glEnableClientState`, `glMatrixMode`, `glLoadMatrixf` | 7 fichiers | OpenGL 3.3 core (phase 6) |
| Boost.Filesystem | parcours des ROMs, chemins | 18 fichiers, ~100 occurrences | `std::filesystem` (phase 3) |
| Boost.DateTime | métadonnées de date, `DateTimeComponent` | 8 fichiers, ~21 occurrences | `std::chrono` (phase 3) |
| Boost.Assign | tables constantes | 5 fichiers | listes d'initialisation (phase 3) |
| Boost.Variant | propriétés de thème | 1 fichier | `std::variant` (phase 3) |
| Boost.Locale | `std::locale::global(generator().generate(""))` dans `main.cpp` | 1 ligne | supprimé, remplacé par la phase 10 |
| Eigen3 | `Vector2f/2i/3f`, `Affine3f` partout | 64 fichiers, ~370 occurrences | glm (phase 4) |
| FreeImage | décodage PNG/JPG, redimensionnement du scraper | 2 fichiers | **retiré des dépôts Arch** → stb_image + libwebp (fait en phase 1) |
| FreeType | rendu des polices | `Font.cpp` | conservé |
| libcurl | `HttpReq` (scrapers) | 1 fichier | conservé |
| ALSA | `VolumeControl` | 1 fichier | conservé (option : PipeWire/PulseAudio plus tard) |
| pugixml (vendored) | XML (settings, gamelists, thèmes, input) | — | conservé, version système possible |
| nanosvg (vendored) | SVG | — | conservé (mise à jour du vendored) |

### 1.3 Problèmes relevés

1. **Build** : `cmake_minimum_required(VERSION 2.8)`, variables globales
   (`include_directories`, `COMMON_LIBRARIES`), build dans l'arbre source, modules `Find*`
   embarqués obsolètes, `-std=c++11 -O3` forcé, détection Raspberry Pi par fichiers.
   Le projet ne compilait plus avec GCC 14 (`<stack>` manquant), ni avec Clang 18
   (`using` de types dépendants sans `typename`). Boost 1.89 a supprimé la bibliothèque
   `boost_system`, ce qui faisait échouer `find_package(Boost COMPONENTS system)` sur Arch.
2. **Bugs latents détectés par Clang** : comparaisons de `const char*` par pointeur
   (`it->first == "up/down"` dans `Window.cpp`, `ComponentGrid.cpp`, `ComponentList.cpp`),
   `delete` d'une classe abstraite sans destructeur virtuel (`Animation`), troncature de
   `std::string::npos` dans `unsigned int` (`Font.cpp`). Corrigés en phase 1.
   Restent : ~50 `-Winconsistent-missing-override`, ~10 `-Wdangling-else`,
   `-Woverloaded-virtual`, une affectation dans une condition (`HttpReq.cpp:120`).
3. **Rendu** : pipeline OpenGL 1.x fixe, un appel de dessin par élément, pas de batching,
   pas de shaders. Incompatible avec un contexte core profile et avec la vidéo YUV efficace.
4. **Entrées** : API `SDL_Joystick` brute avec configuration manuelle de chaque bouton
   (`es_input.cfg`), pas de base `gamecontrollerdb`.
5. **Audio** : ancienne API `SDL_OpenAudio`, mixage manuel de WAV uniquement.
6. **Scrapers** : TheGamesDB legacy (`legacy.thegamesdb.net`) et TheArchive sont hors
   service. À remplacer à terme (ScreenScraper), hors périmètre immédiat.
7. **Thèmes** : `formatVersion` 3 d'Aloshi, vues `system`, `basic`, `detailed`, `grid`.
   Pas de vidéo, pas de sous-ensembles (« subsets »), pas de variables, pas de menus thémés.
8. **Langue** : chaînes d'interface en dur, en anglais.
9. **Chemins en dur** : `getHomePath() + "/.emulationstation/..."` répété dans
   `Settings.cpp`, `InputManager.cpp`, `Log.cpp`, `ThemeData.cpp`, `SystemData.cpp`,
   `Scraper.cpp`, `main.cpp` ; `/etc/emulationstation/...` pour les fichiers système ;
   police de repli `/usr/share/fonts/truetype/droid/...` (chemin Debian) dans `Font.cpp`.
   Aucun respect de la spécification XDG Base Directory.

---

## 2. Principes transverses (valables pour toutes les phases)

- **Une phase = une ou plusieurs PR petites et vertes.** Chaque PR compile avec GCC et
  Clang dans la CI Arch, et l'application démarre (`--help` + test de fumée Xvfb quand il
  existe).
- **Pas de régression fonctionnelle silencieuse** : si une fonctionnalité est retirée
  (ex. Raspberry Pi), c'est écrit dans la PR et dans ce document.
- **Style** : on garde la structure et le nommage existants (`mMember`, `sStatic`,
  `PascalCase` pour les classes, `camelCase` pour les méthodes, tabulations) tant que
  `.clang-format` (phase 2) n'est pas en place ; ensuite on suit `.clang-format`.
- **C++23 effectif** : `std::format`/`std::print`, `std::expected`, `std::span`,
  `std::string_view`, `std::ranges`, `enum class`, `constexpr`, `[[nodiscard]]`,
  `std::unique_ptr` par défaut, `std::jthread`. Pas de `new`/`delete` nus dans le code
  nouveau. Pas de modules C++20 (support CMake/GCC encore fragile).
- **Dépendances** : uniquement des paquets présents dans les dépôts officiels Arch
  (`core`/`extra`), ou des bibliothèques mono-en-tête vendored dans `external/`
  avec leur licence. Pas de dépendance AUR obligatoire.
- **Licences** : le projet est sous licence MIT (Aloshi). Recalbox et Batocera servent de
  **référence fonctionnelle** (formats de thèmes, organisation des traductions) ; ne pas
  copier leur code sans vérifier la compatibilité de licence. On réimplémente les formats.
- **Chemins XDG, aucun chemin en dur** (règle du propriétaire du projet) : tout chemin de
  fichier passe par le module `Paths` (phase 3.A) qui applique la
  [spécification XDG Base Directory](https://specifications.freedesktop.org/basedir-spec/latest/).
  Interdit en dehors de `es-core/src/utils/Paths.cpp` : littéraux `"/etc/..."`,
  `"/usr/..."`, `"/opt/..."`, `"~/.emulationstation"`, `getenv("HOME")`, concaténation
  `getHomePath() + "/..."`. Les chemins d'installation viennent de CMake
  (`GNUInstallDirs`) via un en-tête généré, jamais d'une chaîne tapée à la main.
  Une vérification CI (`grep`) fait échouer la build si un tel littéral réapparaît.
- **Compatibilité des données utilisateur** : `es_systems.cfg`, `gamelist.xml`,
  `es_settings.cfg`, `es_input.cfg` restent lisibles. Toute migration de format est
  automatique et non destructive (sauvegarde `.bak`).

---

## Phase 1 — Build moderne (PR #1)

**Objectif** : un build reproductible, propre et testé en CI sur Arch, sans toucher au
comportement.

Fait :
- `CMakeLists.txt` racine : CMake ≥ 3.25, `project(... VERSION 2.0.0 LANGUAGES C CXX)`,
  `CMAKE_CXX_STANDARD 23`, extensions désactivées, `compile_commands.json`, refus des
  builds dans l'arbre source, binaire produit à la racine du dossier de build.
- Dépendances par cibles importées : `SDL2::SDL2`, `Boost::filesystem`, `Boost::locale`,
  `Boost::headers`, `Eigen3::Eigen`, `Freetype::Freetype`, `CURL::libcurl`, `OpenGL::GL`
  (GLVND), `ALSA::ALSA`, `PkgConfig::WEBP` (libwebp).
- `es-core`, `es-app`, `external/*` : cibles avec `target_include_directories`,
  `target_compile_definitions`, `target_link_libraries`. Inclusion `"Resources.h"` au lieu
  de `"../data/Resources.h"` (qui ne marchait que par accident).
- Options : `ES_WARNINGS_AS_ERRORS`, `ES_ENABLE_SANITIZERS` (ASan + UBSan), `ES_ENABLE_LTO`.
- `CMakePresets.json` : `debug`, `release`, `relwithdebinfo`, `asan`, `clang-debug`.
- `.github/workflows/build.yml` : conteneur `archlinux:base-devel`, matrice GCC/Clang,
  configuration, build, `emulationstation --help`.
- Corrections nécessaires à la compilation C++23 et bugs latents listés en 1.3.
- **FreeImage remplacé** (le paquet n'existe plus dans les dépôts officiels Arch, la CI
  échouait) : `es-core/src/ImageIO.cpp` décode avec **stb_image** (PNG, JPEG, BMP, TGA,
  GIF première image ; vendored dans `external/stb/`) et **libwebp** (WebP statique,
  paquet Arch `libwebp`, via pkg-config). Le redimensionnement des images du scraper
  (`ImageIO::resizeImageFile`) réencode dans le format d'origine (PNG/JPEG via
  stb_image_write, WebP via `WebPEncodeRGBA`). Convention conservée : lignes renvoyées
  de bas en haut, comme l'ancien chargeur FreeImage.
- Suppression : `CMake/` (FindSDL2, FindEigen3, FindOpenGLES, FindFreeImage d'Ogre,
  FindPkgMacros), détection RPi/Mali, réglages MSVC, CPack Debian.

Critères d'acceptation : CI verte (GCC + Clang), `cmake --preset release && cmake --build
--preset release` fonctionne sur une Arch à jour, l'application démarre.

Note : `stb_image_resize2.h` n'est plus publié dans le dépôt stb ; le redimensionnement
utilise une moyenne par zone écrite dans `ImageIO.cpp` (suffisant pour des vignettes).

---

## Phase 2 — Hygiène C++23 et outillage

**Objectif** : rendre le code sûr à modifier en masse dans les phases suivantes.

### Tâches
1. **Formatage** : ajouter `.clang-format` reproduisant le style existant (tabulations,
   accolades Allman pour fonctions/classes, largeur 120). Reformater en **un seul commit
   dédié** (`style: apply clang-format`), puis ajouter son hash à `.git-blame-ignore-revs`.
2. **Analyse statique** : `.clang-tidy` avec `modernize-*`, `bugprone-*`,
   `performance-*`, `readability-braces-around-statements`, `cppcoreguidelines-virtual-class-destructor`
   (exclure `modernize-use-trailing-return-type`, `readability-identifier-length`,
   `cppcoreguidelines-avoid-magic-numbers`). Cible CMake `es-tidy` optionnelle.
3. **Avertissements** : corriger tout ce que GCC 14 et Clang 18 signalent avec
   `-Wall -Wextra` (notamment `override` manquants, `dangling-else`, `overloaded-virtual`,
   affectation dans `HttpReq.cpp:120`). Puis activer `ES_WARNINGS_AS_ERRORS` en CI.
4. **Modernisations mécaniques** (clang-tidy `--fix`, relu à la main) :
   `override`, `nullptr`, `using` au lieu de `typedef`, `= default`, boucles `for` sur
   plages, `emplace_back`, `#pragma once` partout, `enum` → `enum class` quand l'énumération
   n'est pas utilisée comme masque de bits.
5. **Propriété mémoire** : recenser les `new`/`delete` nus (`grep -rn "new \|delete "`)
   et les remplacer par `std::unique_ptr`/`std::make_unique` ; `Window::pushGui`/`GuiComponent`
   gardent la sémantique actuelle mais documentent qui possède quoi.
6. **Journalisation** : réécrire `Log` (`es-core/src/Log.{h,cpp}`) avec `std::format`,
   niveaux `Error/Warning/Info/Debug`, macro `LOG(level)` conservée pour compatibilité,
   sortie fichier `es_log.txt` (emplacement fourni par `Paths` à partir de la phase 3.A ;
   d'ici là, chemin actuel inchangé) + `stderr` en mode `--debug`.
   Pas de dépendance externe (spdlog possible mais non nécessaire).
7. **Tests** : ajouter `tests/` avec **Catch2 v3** (paquet Arch `catch2`), option CMake
   `ES_BUILD_TESTS` (ON en CI), `enable_testing()` + `catch_discover_tests`. Premiers tests :
   `Util` (chemins, chaînes), `MetaDataList`, parsing de `gamelist.xml`, parsing de thèmes
   sur des fixtures `tests/data/`.
8. **Test de fumée graphique en CI** : lancer l'application sous `xvfb-run` avec un
   `es_systems.cfg` minimal (voir `tests/smoke/`), tuer après 5 s, échouer si le log contient
   `lvl0` (erreur) ou si le processus s'arrête avant.

### Critères d'acceptation
- Zéro avertissement GCC/Clang dans `es-core` et `es-app` ; `-Werror` actif en CI.
- `ctest --preset debug` vert en CI.
- Aucun changement de comportement visible.

---

## Phase 3 — Suppression de Boost

**Objectif** : n'utiliser que la bibliothèque standard C++23.

### 3.A Module de chemins XDG (à faire en premier dans la phase)

Nouveau `es-core/src/utils/Paths.{h,cpp}`, seul endroit du code qui connaît des chemins.

| Rôle | Variable XDG (défaut) | Dossier ES | Contenu |
|------|----------------------|------------|---------|
| Configuration utilisateur | `$XDG_CONFIG_HOME` (`~/.config`) | `emulationstation/` | `es_settings.cfg`, `es_input.cfg`, `es_systems.cfg` (surcharge), `es_theme_default.xml`, `gamecontrollerdb.txt` |
| Données utilisateur | `$XDG_DATA_HOME` (`~/.local/share`) | `emulationstation/` | `themes/`, `gamelists/<système>/gamelist.xml`, `downloaded_images/` (médias scrapés), `music/` |
| État | `$XDG_STATE_HOME` (`~/.local/state`) | `emulationstation/` | `es_log.txt` (+ rotation `es_log.txt.1`) |
| Cache | `$XDG_CACHE_HOME` (`~/.cache`) | `emulationstation/` | cache de vignettes/textures, cache HTTP du scraper |
| Configuration système | `$XDG_CONFIG_DIRS` (`/etc/xdg`) | `emulationstation/` | `es_systems.cfg` par défaut de l'administrateur |
| Données système | `$XDG_DATA_DIRS` (`/usr/local/share:/usr/share`) + préfixe d'installation | `emulationstation/` | `themes/`, `resources/`, `locale/`, `es_systems.cfg` d'exemple |

API proposée :
```cpp
namespace es::paths {
enum class Kind { Config, Data, State, Cache };
std::filesystem::path userDir(Kind);                       // crée le dossier (0700) si absent
std::filesystem::path userFile(Kind, std::string_view rel); // userDir(kind) / rel
std::vector<std::filesystem::path> systemDirs(Kind);       // Config → XDG_CONFIG_DIRS, Data → XDG_DATA_DIRS + préfixe d'installation
std::optional<std::filesystem::path> find(Kind, std::string_view rel); // utilisateur d'abord, puis système, dans l'ordre XDG
std::filesystem::path expandUser(std::string_view);        // « ~ » et « ~/ » dans es_systems.cfg et les thèmes
}
```

Règles :
- Une variable XDG vide ou **relative** est ignorée (exigence de la spécification) et la
  valeur par défaut s'applique ; le défaut se calcule à partir de `$HOME`, ou de
  `getpwuid(getuid())->pw_dir` si `HOME` est absent.
- Préfixe d'installation : `configure_file(Paths.config.h.in)` injecte
  `CMAKE_INSTALL_FULL_DATADIR` et `CMAKE_INSTALL_FULL_SYSCONFDIR` ; on cherche aussi
  `<dossier de l'exécutable>/../share/emulationstation` (lancement depuis le dossier de
  build ou une installation relocalisée) via `/proc/self/exe`.
- Mode portable / tests : `--home <dossier>` ou variable `ES_HOME` force les quatre
  dossiers utilisateur dans `<dossier>/{config,data,state,cache}`.
- **Migration** depuis `~/.emulationstation/` : au premier lancement, si l'ancien dossier
  existe et que les nouveaux sont vides, **copier** (jamais déplacer) chaque fichier vers
  son nouveau dossier selon la table, journaliser chaque copie, et écrire un fichier
  `~/.emulationstation/MIGRATED_TO_XDG.txt` expliquant où sont les fichiers. Ne jamais
  supprimer l'ancien dossier.
- Les chemins relatifs des `gamelist.xml` (`./media/...`) restent relatifs au dossier
  de ROMs du système, comme aujourd'hui.
- Police de repli : ne plus viser `/usr/share/fonts/...` ; utiliser **fontconfig**
  (`FcFontMatch`, paquet Arch `fontconfig`) pour trouver une police couvrant le glyphe
  manquant.
- Remplacer chaque site d'appel listé en 1.3 point 9 ; supprimer `getHomePath()`.
- Tests unitaires : variables XDG définies/absentes/relatives, `ES_HOME`, `expandUser`,
  migration sur un faux `$HOME` temporaire.
- CI : étape « no-hardcoded-paths » :
  `! grep -rnE '"/(etc|usr|opt)/|\.emulationstation|getenv\("HOME"\)' es-core/src es-app/src --exclude=Paths.cpp`.

### Correspondances
| Boost | Remplacement | Points d'attention |
|-------|--------------|--------------------|
| `boost::filesystem::path` | `std::filesystem::path` | `path::generic_string()` identique ; `fs::canonical` lève si le chemin n'existe pas → utiliser `weakly_canonical` ; `boost::filesystem::directory_iterator` → `std::filesystem::directory_iterator` avec `std::error_code` pour ne pas lever sur un dossier illisible |
| `boost::filesystem::exists/is_directory/...` | idem dans `std::filesystem` | utiliser les surcharges `std::error_code` dans `SystemData::populateFolder` |
| `boost::posix_time::ptime` | `std::chrono::sys_seconds` (ou `local_seconds`) | format ISO `YYYYMMDDTHHMMSS` des `gamelist.xml` à préserver exactement |
| `boost::posix_time::to_iso_string` | `std::format("{:%Y%m%dT%H%M%S}", tp)` | |
| `string_to_ptime(str, fmt)` | `std::chrono::parse` (libstdc++ 14+) | formats utilisés : `"%Y%m%dT%H%M%S%F%q"` (ISO), `"%m/%d/%Y"` (TheGamesDB) ; écrire des tests |
| `boost::gregorian::day_clock::local_day()` | `std::chrono::floor<std::chrono::days>(system_clock::now())` | |
| `boost::posix_time::not_a_date_time` | `std::optional<sys_seconds>` ou valeur sentinelle `sys_seconds{}` | |
| `boost::posix_time::time_facet` (affichage `DateTimeComponent`) | `std::format` avec le format du thème | conversion des spécificateurs Boost (`%Y-%m-%d`) qui sont compatibles strftime |
| `boost::assign::map_list_of` / `list_of` | listes d'initialisation `{ {"a", x}, ... }` | `ThemeData::sElementMap` → `static const std::map<std::string, ElementMapType>` ; supprimer `makeMap` |
| `boost::variant<...>` | `std::variant<...>` | `ThemeElement::get<T>` → `std::get<T>` ; `boost::get` lève `bad_get`, `std::get` lève `bad_variant_access` |
| `boost::locale::generator` | supprimé | `std::setlocale(LC_ALL, "")` ; la traduction arrive en phase 10 |

### Fichiers concernés (liste à vérifier par `grep -rln boost es-core es-app`)
`es-core/src/{Util,Settings,ThemeData,platform,InputManager,AudioManager}.{h,cpp}`,
`es-core/src/components/{DateTimeComponent,HelpComponent}.*`,
`es-core/src/resources/{ResourceManager,Font}.*`, `es-app/src/{main,SystemData,FileData,
Gamelist,MetaData,ScraperCmdLine}.*`, `es-app/src/scrapers/*`, `es-app/src/guis/*`.

### Tâches
1. Introduire `namespace fs = std::filesystem;` dans un en-tête commun (`Util.h`).
2. Migrer fichier par fichier, compiler après chaque fichier.
3. Supprimer `Boost` de `CMakeLists.txt` et de `es-core/CMakeLists.txt`.
4. Ajouter des tests sur les conversions de dates (aller-retour ISO, dates vides,
   fuseau horaire : les dates des gamelists sont **sans fuseau**, à traiter comme UTC).

### Critères d'acceptation
- Tous les fichiers sont lus/écrits aux emplacements XDG ; l'étape CI
  « no-hardcoded-paths » est verte ; la migration depuis `~/.emulationstation/` est testée.
- `grep -rn boost es-core es-app` ne renvoie rien ; `find_package(Boost)` supprimé.
- Une gamelist existante est relue puis réécrite **à l'identique** (test de non-régression
  sur fixture).

---

## Phase 4 — Remplacement d'Eigen

**Objectif** : supprimer une dépendance lourde utilisée seulement pour des vecteurs 2D/3D
et des transformations affines.

### Choix
**glm** (paquet Arch `glm`, en-têtes seuls, conçu pour OpenGL, matrices colonnes compatibles
avec les uniformes de la phase 6). Alternative écartée : classes maison type Batocera
(`Vector2f`, `Transform4x4f`), plus de code à maintenir.

### Correspondances
| Eigen | glm |
|-------|-----|
| `Eigen::Vector2f/2i/3f/4f` | `glm::vec2/ivec2/vec3/vec4` |
| `v.x()`, `v.y()` | `v.x`, `v.y` |
| `Eigen::Affine3f` | `glm::mat4` |
| `Affine3f::Identity()` | `glm::mat4(1.0f)` |
| `t.translate(v)` | `t = glm::translate(t, v)` |
| `t.scale(v)` / `rotate(AngleAxisf(a, Vector3f::UnitZ()))` | `glm::scale`, `glm::rotate(t, a, {0,0,1})` |
| `t.translation()` | `glm::vec3(t[3])` |
| `t.data()` | `glm::value_ptr(t)` |
| `v.cwiseProduct(w)` | `v * w` |
| `Eigen::Vector2f::Zero()` | `glm::vec2(0.0f)` |

Attention : en Eigen, `a * b` sur `Affine3f` compose dans le même sens que glm, mais
`translate` d'Eigen **post-multiplie** (comme glm) alors que `pretranslate` pré-multiplie :
vérifier chaque `pretranslate`/`prescale`. Comparer des captures d'écran avant/après
(test de fumée Xvfb + `import -window root`) pour détecter un décalage.

### Tâches
1. Ajouter `es-core/src/math/Math.h` qui inclut glm et définit les alias
   `using Vector2f = glm::vec2;` etc. **pour la transition**, puis migrer.
2. Supprimer `EIGEN_DONT_ALIGN` et `Eigen3` de CMake.

### Critères d'acceptation
- Aucune occurrence `Eigen` ; rendu identique au pixel près sur les captures de référence
  (tolérance d'anticrénelage acceptée).

---

## Phase 5 — Migration SDL2 → SDL3

**Référence obligatoire** : `docs/README-migration.md` du dépôt SDL (version 3.2+). Paquet
Arch : `sdl3`.

### 5.1 Initialisation, fenêtre, contexte
- `SDL_Init` renvoie désormais `bool` (true = succès). Revoir **tous** les tests de retour
  (`< 0` → `!`).
- `SDL_CreateWindow(title, w, h, flags)` (plus de x/y). Plein écran : `SDL_WINDOW_FULLSCREEN`
  + `SDL_SetWindowFullscreenMode(window, nullptr)` pour le plein écran « bureau ».
- Contexte GL demandé en **3.3 core** dès cette phase (`SDL_GL_CONTEXT_PROFILE_CORE`) mais
  seulement si la phase 6 est livrée dans la même série ; sinon contexte compatibilité.
- `SDL_GL_SetSwapInterval` renvoie `bool`. VSync adaptatif (-1) puis repli sur 1.
- Gestion du DPI : `SDL_WINDOW_HIGH_PIXEL_DENSITY`, `SDL_GetWindowSizeInPixels` pour le
  viewport. `Renderer::getScreenWidth/Height` doit renvoyer les pixels.
- Fichiers : `es-core/src/Renderer_init_sdlgl.cpp`, `Renderer.h`, `Window.cpp`, `main.cpp`.

### 5.2 Événements
- Renommage des événements (`SDL_KEYDOWN` → `SDL_EVENT_KEY_DOWN`, `SDL_QUIT` →
  `SDL_EVENT_QUIT`, `SDL_JOYAXISMOTION` → `SDL_EVENT_JOYSTICK_AXIS_MOTION`, etc.).
- `event.key.keysym.sym` → `event.key.key` ; `SDL_TEXTINPUT` nécessite
  `SDL_StartTextInput(window)` explicite (le faire dans `TextEditComponent` à l'entrée en
  édition, `SDL_StopTextInput` à la sortie).
- Utiliser `SDL_EVENT_WINDOW_*` pour le redimensionnement en mode fenêtré.

### 5.3 Manettes
- Passer de `SDL_Joystick` à **`SDL_Gamepad`** (anciennement GameController) :
  boutons sémantiques (`SDL_GAMEPAD_BUTTON_SOUTH`…), mappage par la base intégrée SDL +
  fichier optionnel `gamecontrollerdb.txt` du dossier de configuration XDG chargé par
  `SDL_AddGamepadMappingsFromFile`.
- Garder la configuration manuelle (`GuiInputConfig`, `es_input.cfg`) **en repli** pour les
  joysticks non reconnus : `InputConfig` conserve son format XML ; un périphérique reconnu
  comme gamepad reçoit une configuration par défaut générée (A=South, B=East, etc. ;
  option pour inverser A/B, comme Recalbox « style Nintendo »).
- Branchement à chaud : `SDL_EVENT_GAMEPAD_ADDED/REMOVED`, `SDL_EVENT_JOYSTICK_ADDED/REMOVED`.
  Les ID d'instance sont des `SDL_JoystickID` (Uint32), plus des index.
- Fichiers : `InputManager.{h,cpp}`, `InputConfig.{h,cpp}`, `guis/GuiDetectDevice.cpp`,
  `guis/GuiInputConfig.cpp`.

### 5.4 Audio (base)
- `SDL_OpenAudio` + rappel → `SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
  &spec, callback, userdata)` ; mixage logiciel conservé jusqu'à la phase 8.
- `SDL_LoadWAV` renvoie `bool` ; `SDL_MixAudio` prend désormais un format et un volume flottant.
- Fichiers : `AudioManager.{h,cpp}`, `Sound.{h,cpp}`.

### 5.5 Divers
- `SDL_GetTicks()` renvoie `Uint64` (ms) ; adapter les types dans `Window`, animations.
- `SDL_ShowCursor`/`SDL_HideCursor`.
- CMake : `find_package(SDL3 CONFIG REQUIRED)` + `SDL3::SDL3`.

### Critères d'acceptation
- Plus aucune référence à `SDL2`. Navigation au clavier et à la manette (Xbox, DualSense,
  8BitDo testés si possible), branchement à chaud, saisie de texte, sons de navigation.

---

## Phase 6 — Moteur de rendu OpenGL 3.3 core

**Objectif** : un rendu moderne, batché, à shaders, prérequis pour la vidéo, les effets de
thèmes (transparence, rotation, dégradés, coins arrondis) et les performances.

### Conception
- Chargeur de fonctions : **glad 2** généré pour GL 3.3 core (vendored dans
  `external/glad/`, licence MIT/Apache) ; ou `SDL_GL_GetProcAddress` + en-tête maison.
- API interne (`es-core/src/renderer/`) :
  - `Renderer` (singleton existant) garde ses fonctions publiques
    (`pushClipRect`, `popClipRect`, `setMatrix`, `drawRect`) pour limiter l'impact.
  - Nouveau `Renderer::drawTriangles(std::span<const Vertex>, TextureHandle, BlendMode)`.
  - `struct Vertex { glm::vec2 pos; glm::vec2 tex; uint32_t color; };`
  - Un **batcher** accumule les sommets tant que texture/shader/blend/clip ne changent pas ;
    flush sur changement d'état ou en fin d'image.
  - Shaders : `texture_rgba` (images, polices), `color` (rectangles), `yuv420p`
    (vidéo, 3 textures `R8`), `nv12` (vidéo VA-API). Sources GLSL `#version 330 core`
    embarquées en `constexpr std::string_view`.
  - Matrice de projection orthographique + matrice modèle-vue en uniformes ; la pile de
    transformations des `GuiComponent` est conservée (multiplication CPU).
  - Clip : `glScissor` (comme aujourd'hui) ; prévoir le stencil plus tard pour les
    rotations.
  - Textures : `TextureResource` crée des `GL_RGBA8` avec mipmaps optionnels,
    `GL_CLAMP_TO_EDGE`, `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` pour les polices.
- Polices (`Font.cpp`) : l'atlas devient `GL_R8` échantillonné avec un swizzle
  (`GL_TEXTURE_SWIZZLE_RGBA`) ou un shader dédié.
- Mode debug : `GL_KHR_debug` → messages dans le log en build Debug.

### Fichiers concernés
`Renderer.h`, `Renderer_draw_gl.cpp`, `Renderer_init_sdlgl.cpp`, `platform.h`,
`components/{ImageComponent,NinePatchComponent,TextComponent,...}`,
`resources/{Font,TextureResource}.cpp`, `es-app/src/components/RatingComponent.cpp`,
tout appel direct à `gl*` (`grep -rn "gl[A-Z]" es-core es-app`).

### Critères d'acceptation
- Aucun appel GL hors de `es-core/src/renderer/`.
- Contexte core profile 3.3 ; aucune erreur `GL_KHR_debug` en Debug.
- 60 i/s stables sur un iGPU Intel/AMD en 1080p et 4K avec une gamelist de 1 000 jeux.
- Captures identiques à la phase 4 (tolérance).

---

## Phase 7 — Images : formats animés, chargement asynchrone, cache

> Hypothèse : « stp » dans la demande initiale désigne **stb** (bibliothèques mono-en-tête
> de Sean Barrett). À confirmer avec le propriétaire du projet.

### Acquis de la phase 1
stb_image (PNG, JPEG, BMP, TGA, GIF), libwebp (WebP statique), nanosvg (SVG),
`ImageIO::loadFromMemoryRGBA32` et `ImageIO::resizeImageFile`.

### Compléments
- **WebP animé** via `libwebpdemux` (`WebPAnimDecoder`, module pkg-config `libwebpdemux`).
- **GIF animé** via `stbi_load_gif_from_memory` (renvoie toutes les images et les délais).
- **nanosvg** : mettre à jour la copie vendored ; option future lunasvg si les thèmes
  Recalbox l'exigent.
- Option ultérieure : AVIF via `libavif` (paquet Arch) si des thèmes l'utilisent.

### Conception
- `ImageIO` devient une façade : `std::expected<Image, std::string> ImageIO::load(std::span<const std::byte>)`
  avec `struct Image { int width, height; std::vector<uint8_t> rgba; std::vector<Frame> frames; }`.
  Détection du format par signature (`RIFF....WEBP`, `\x89PNG`, `\xFF\xD8`, `GIF8`, `<svg`/`<?xml`).
- **Chargement asynchrone** : un pool de threads (`std::jthread`, 2-4 workers) décode les
  images hors du thread de rendu ; seul l'envoi GL (`glTexImage2D`) se fait sur le thread
  principal. `TextureResource` affiche un placeholder transparent tant que l'image n'est
  pas prête.
- **Cache** : budget mémoire VRAM configurable (`MaxVRAM` en Mo dans `es_settings.cfg`,
  défaut 256), éviction LRU des textures non visibles.
- `AnimatedImageComponent` gère les WebP/GIF animés (durées par image).

### Critères d'acceptation
- PNG/JPG/WebP (statique et animé)/GIF animé/SVG s'affichent.
- Pas de saccade en défilement rapide d'une gamelist de 1 000 jeux avec images.
- Tests unitaires de décodage sur fixtures `tests/data/images/`.

---

## Phase 8 — Audio

### Choix
Décodage via **FFmpeg** (déjà requis en phase 9 : `libavformat`, `libavcodec`,
`libswresample`) pour WAV, OGG, MP3, FLAC, OPUS ; sortie via **`SDL_AudioStream`** de SDL3.
Cela évite une dépendance supplémentaire (SDL3_mixer). Alternative légère si l'on veut
découpler de FFmpeg : `dr_wav`/`dr_mp3`/`stb_vorbis` vendored.

### Fonctionnalités
- Sons de navigation (thème : élément `sound`), préchargés et décodés en PCM float.
- **Musique de fond** façon Recalbox : dossier `music/` de `$XDG_DATA_HOME/emulationstation/`
  (et de `$XDG_DATA_DIRS`), et musiques du thème, lecture aléatoire, fondu, pause pendant la vidéo et pendant le jeu, réglage du
  volume dans le menu Son. Options `BackgroundMusic` (bool), `MusicVolume` (0-100).
- `VolumeControl` (ALSA) conservé ; ajouter plus tard un backend PipeWire/PulseAudio
  (`libpulse`) car Arch utilise PipeWire par défaut (ALSA reste accessible via pipewire-alsa).

### Critères d'acceptation
- Plus d'`SDL_MixAudio` maison ; mixage par plusieurs `SDL_AudioStream` liés au même
  périphérique logique. Aucun craquement ; latence < 50 ms pour les sons de navigation.

---

## Phase 9 — Vidéo avec FFmpeg

### Fonctionnalités
- Élément de thème **`video`** (vue `detailed`/`video` et vue système), lecture du champ
  `<video>` des `gamelist.xml` (convention Recalbox/Batocera/ES-DE : `./media/videos/xxx.mp4`).
- Délai avant lecture (`delay`, défaut 2 s), image de remplacement pendant le délai
  (`default` / image du jeu), lecture en boucle, son de la vidéo optionnel (`VideoAudio`).
- Économiseur d'écran « clips de jeux » (gameclip façon Recalbox) : lecture aléatoire de
  vidéos de la collection avec bandeau d'information.

### Conception
- Paquet Arch `ffmpeg` ; CMake via `pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavformat libavcodec libavutil libswscale libswresample)`.
- `es-core/src/video/VideoPlayer.{h,cpp}` :
  - thread de démultiplexage (`av_read_frame`) → files de paquets audio/vidéo bornées ;
  - thread de décodage vidéo → file de `AVFrame` (3 à 5 images) ;
  - décodage audio → `swr_convert` en float stéréo 48 kHz → `SDL_AudioStream` ;
  - **horloge maîtresse = audio** quand il y a du son, sinon horloge système ;
    abandon des images en retard, attente des images en avance ;
  - accélération matérielle optionnelle **VA-API** (`av_hwdevice_ctx_create(AV_HWDEVICE_TYPE_VAAPI)`),
    repli logiciel automatique ; option `VideoHardwareDecoding`.
- `VideoComponent` (es-core/components) : sur le thread principal, envoie Y/U/V dans trois
  textures `GL_R8` (`glTexSubImage2D`, PBO en option) et dessine avec le shader `yuv420p`.
  Formats non YUV420P convertis par `sws_scale`.
- Arrêt propre : `std::jthread` + `std::stop_token`, aucune fuite à la destruction d'un
  composant en cours de lecture (navigation rapide dans la liste).

### Critères d'acceptation
- Vidéo H.264 720p et H.265 1080p fluides, son synchronisé (< 80 ms d'écart).
- Navigation rapide sans fuite (vérifier avec le preset `asan`) ni blocage de l'interface.

---

## Phase 10 — Internationalisation (gettext .pot/.po/.mo)

### Modèle
Comme Recalbox et Batocera : catalogues gettext standards, domaine **`emulationstation2`**,
langue choisie dans le menu (« Langue / Language ») et enregistrée dans
`es_settings.cfg` (`Language`, ex. `fr_FR`).

### Arborescence
```
locale/
  emulationstation2.pot           # modèle généré, versionné
  lang/
    fr/LC_MESSAGES/emulationstation2.po
    de/LC_MESSAGES/emulationstation2.po
    ...
```
Installation : `${CMAKE_INSTALL_DATADIR}/emulationstation/locale/<lang>/LC_MESSAGES/emulationstation2.mo`,
trouvé à l'exécution par `Paths::find(Kind::Data, "locale/...")` (catalogues utilisateur
prioritaires dans `$XDG_DATA_HOME/emulationstation/locale/`).

### Code
- En-tête `es-core/src/utils/I18n.h` :
  - `_("texte")` → traduction (singulier) ;
  - `_N("%i jeu", "%i jeux", n)` → pluriel (`ngettext`) ;
  - `_P("contexte", "texte")` → contexte (`pgettext`) ;
  - `_U("…")` pour les chaînes affichées en majuscules (les menus ES sont en capitales :
    traduire puis passer en majuscules **après** traduction, avec une fonction
    `toUpperUtf8` qui gère les accents).
- **Moteur d'exécution** : lecteur `.mo` interne (`MoCatalog`, ~200 lignes : en-tête
  magique `0x950412de`, tables de chaînes, recherche par table de hachage ou `std::unordered_map`,
  évaluation de `Plural-Forms` par un petit analyseur d'expressions C). Raison : la gettext
  de glibc ignore `LANGUAGE` quand la locale système est `C`/`POSIX`, ce qui est fréquent
  en session kiosque ; Batocera a rencontré le même problème. Le lecteur interne ne dépend
  pas des locales générées par `locale-gen`.
- Formatage des chaînes traduites : `std::vformat(_("{} jeux"), std::make_format_args(n))`
  — les traducteurs voient des `{}` ; éviter la concaténation de morceaux de phrases.
- Changement de langue à chaud : recharger le catalogue puis reconstruire les menus et
  vues (`ViewController::reloadAll()`).

### Outillage CMake
- `find_package(Gettext REQUIRED)` + `find_program(XGETTEXT_EXECUTABLE xgettext)`.
- Cible **`es-update-pot`** : `xgettext --language=C++ --from-code=UTF-8 --keyword=_
  --keyword=_N:1,2 --keyword=_P:1c,2 --keyword=_U --add-comments=TRANSLATORS:
  --package-name=emulationstation-re -o locale/emulationstation2.pot <sources>`.
- Cible **`es-update-po`** : `msgmerge --update --backup=none` pour chaque `.po`.
- Compilation des `.mo` à chaque build (`msgfmt --check -o`) et installation.
- CI : vérifier que `.pot` est à jour (`es-update-pot` puis `git diff --exit-code`,
  en ignorant la ligne `POT-Creation-Date`).

### Tâches
1. Marquer **toutes** les chaînes d'interface de `es-app` et `es-core` (menus, aides,
   messages, noms de métadonnées). Ne pas traduire : clés de configuration, noms d'éléments
   de thème, journaux.
2. Fournir `fr` complet ; `en` est la langue source.
3. Polices : vérifier la couverture des glyphes (fallback `noto-fonts`, `noto-fonts-cjk`
   pour CJK ; la recherche de police de repli de `Font.cpp` cible aujourd'hui `fonts-droid`).

### Critères d'acceptation
- Interface entièrement en français quand `Language=fr_FR`, même avec `LANG=C`.
- `.pot` régénérable, `.po` fusionnables, CI de cohérence verte.

---

## Phase 11 — Thèmes sur le modèle de Recalbox 10

> Recalbox est la référence fonctionnelle. **Avant d'implémenter**, l'agent doit étudier
> le code et la documentation du moteur de thèmes de Recalbox (dépôt GitLab
> `recalbox/recalbox-emulationstation`, branche correspondant à Recalbox 10, et le thème
> par défaut `recalbox-next`) pour confirmer chaque point ci-dessous. Ce plan liste ce qui
> est attendu à haut niveau ; les détails de syntaxe sont à vérifier sur la source, pas à
> inventer. Ne pas copier de code sans vérifier la licence.

### 11.1 Compatibilité
- Conserver le format Aloshi v3 (thèmes existants), ajouter le format Recalbox
  (`formatVersion` supérieur), détecté par `<formatVersion>`.
- Emplacements des thèmes : `themes/` dans `$XDG_DATA_HOME/emulationstation/` puis dans chaque
  `$XDG_DATA_DIRS/emulationstation/` (via `Paths::find`).

### 11.2 Fonctionnalités attendues (à confirmer sur la source Recalbox)
- **Options de thème (« subsets »)** : jeux de couleurs, d'icônes, de menus, variantes de
  vue système et de gamelist, sélectionnables dans un menu « Options du thème » et
  enregistrés par thème dans `es_settings.cfg`. Inclusion conditionnelle des fichiers XML
  selon l'option choisie.
- **Variables** : `$system`, variables définies par le thème, substitution dans les
  chemins et valeurs.
- **Conditions par résolution / orientation / petit écran** (attributs de type
  `tinyScreen`, `verticalScreen`, et résolution cible) et **par région/langue**.
- **Vues** : `system` (carrousel), `basic`, `detailed`, `grid`, `video`, `menu`,
  `gameclip` (économiseur d'écran), et écran de démarrage.
- **Éléments** : `image`, `text`, `textlist`, `datetime`, `rating`, `video`, `carousel`,
  `ninepatch`, `container`, `sound`, `helpsystem`, `menu*` (fond, titre, éléments,
  icônes, police des menus), avec les propriétés de Recalbox (`zIndex`, rotation et
  origine de rotation, `opacity`, `color`/`colorEnd`/dégradés, `reflection`, `path`
  multiples avec repli, `default`, `alignment`, `forceUppercase`, `lineSpacing`, etc.).
- **Menus thémés** : couleurs, polices, icônes de menu fournis par le thème.
- **Carrousel système** : horizontal/vertical, nombre de logos visibles, échelle, logo de
  repli texte, transitions.
- **Rechargement à chaud** du thème quand on change d'option, sans redémarrage.

### 11.3 Conception
- Réécrire `ThemeData` en trois couches :
  1. **Chargeur** : lit le XML, résout `<include>`, variables et conditions → arbre
     d'éléments normalisé.
  2. **Schéma** : table déclarative des éléments et types de propriétés
     (`std::variant<glm::vec2, std::string, uint32_t, float, bool, PathList>`), avec
     messages d'erreur précis (fichier:ligne, via `pugixml::xml_node::offset_debug`).
  3. **Application** : `applyTheme()` de chaque composant, inchangé dans son principe.
- Validation non bloquante : un élément inconnu produit un avertissement, pas un échec du
  thème entier.
- Thème par défaut livré avec le projet (créé pour le projet, sous licence libre, pas
  une copie de `recalbox-next`) dans `themes/es-re-default/`.

### 11.4 Critères d'acceptation
- Un thème Aloshi v3 existant (ex. `simple`) s'affiche comme avant.
- Le thème Recalbox de référence se charge sans erreur bloquante et ses options sont
  sélectionnables ; écarts documentés dans `docs/THEMES.md`.
- Documentation du format pour les créateurs : `docs/THEMES.md`.

---

## Phase 12 — Distribution Arch

1. **Chemins** : déjà XDG depuis la phase 3.A ; ici on vérifie seulement que le paquet
   installe dans `/usr/share/emulationstation/` et `/etc/xdg/emulationstation/` et que
   `Paths` les trouve.
2. **Installation** : binaire `bin/emulationstation`, ressources
   `share/emulationstation/{resources,themes,locale}`, `.desktop`, icône, `es_systems.cfg`
   d'exemple dans `share/emulationstation/`.
3. **PKGBUILD** dans `dist/arch/PKGBUILD` (paquet `emulationstation-re-git`) avec
   `depends=(sdl3 freetype2 curl ffmpeg libwebp alsa-lib libglvnd)` et
   `makedepends=(cmake ninja glm gettext)`. Test `namcap`.
4. **Session kiosque** : exemple d'unité systemd utilisateur / session Wayland via
   `cage` (paquet Arch) pour démarrer directement sur ES.
5. **Lancement des jeux** : libérer le contexte GL et l'audio avant `system()`, restaurer
   après (comportement actuel à conserver avec SDL3 ; vérifier sous Wayland).

---

## Hors périmètre immédiat (backlog)

- Scraper ScreenScraper (remplace TheGamesDB legacy et TheArchive, morts).
- Collections automatiques (favoris, derniers joués) et personnalisées.
- Backend audio PipeWire natif.
- Wayland natif vérifié (SDL3 le gère ; tester sous KDE/GNOME/`cage`).
- Rendu via `SDL_GPU` (Vulkan) si OpenGL devient limitant.

---

## Annexe A — Paquets Arch par phase

| Phase | Paquets (`pacman -S --needed`) |
|-------|--------------------------------|
| 1 | `base-devel cmake ninja clang sdl2-compat boost freetype2 eigen curl alsa-lib libglvnd mesa libwebp` |
| 2 | + `catch2 xorg-server-xvfb` |
| 3 | − `boost`, + `fontconfig` |
| 4 | − `eigen`, + `glm` |
| 5 | − `sdl2-compat`, + `sdl3` |
| 8-9 | + `ffmpeg` (et `libva` pour VA-API) |
| 10 | + `gettext` |

## Annexe B — Commandes de référence

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug                         # à partir de la phase 2
cmake --preset asan && cmake --build --preset asan
cmake --build --preset debug --target es-update-pot   # à partir de la phase 10
./build/debug/emulationstation --windowed --resolution 1280 720 --debug
```
