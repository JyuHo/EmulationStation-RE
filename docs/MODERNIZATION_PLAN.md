# Plan de modernisation d'EmulationStation-RE

> Document de référence. Il décrit l'état de départ, les choix techniques, et chaque
> phase de travail avec ses tâches, les fichiers concernés, ses critères d'acceptation
> et ses risques. Les phases sont ordonnées : chacune suppose les précédentes terminées.
> Un agent (humain ou IA) qui reprend le travail doit lire aussi [`PROMPT.md`](../PROMPT.md).
> Le dernier audit de l'état réel du dépôt est dans [`AUDIT.md`](AUDIT.md) (31/10/2026).

---

## 0. Résumé

| Phase | Sujet | Dépend de | Taille estimée |
|------:|-------|-----------|----------------|
| 1 | Build moderne : CMake 3.25+, C++23, presets, CI Arch ; FreeImage → stb_image + libwebp | — | S (fait, PR #1) |
| 2 | Hygiène C++23 et outillage (clang-format, clang-tidy, tests, logs) | 1 | M (fait, PR #2) |
| 3 | Suppression de Boost + module de chemins **XDG** (std::filesystem, std::chrono, std::variant) | 2 | M (**fait** : 3.A à 3.E) |
| 4 | Remplacement d'Eigen par une petite bibliothèque mathématique (glm) | 2 | M (**fait** : PR #30 à #32) |
| 5 | Migration SDL2 → SDL3 (fenêtre, événements, manettes, audio) | 3, 4 | L (**fait** : PR #33, #34, #37) |
| 6 | Nouveau moteur de rendu OpenGL 3.3 core à shaders | 5 | L (**en cours** : reste la mesure sur iGPU) |
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
| Eigen3 | `Vector2f/2i/3f`, `Affine3f` partout | 64 fichiers, ~370 occurrences | glm (fait en phase 4) |
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
- **Bibliothèques imposées** (choix du propriétaire du projet) :
  - images : **stb** (`stb_image`, `stb_image_write`, et les autres en-têtes stb au besoin) ;
  - JSON : **nlohmann/json** exclusivement (paquet Arch `nlohmann-json`,
    `find_package(nlohmann_json 3.11 CONFIG REQUIRED)`, cible
    `nlohmann_json::nlohmann_json`). Pas d'autre bibliothèque JSON, pas de parseur maison.
    La dépendance est ajoutée au build avec son premier usage réel, pas avant.
    Usages prévus : réponses JSON des scrapers (ScreenScraper), métadonnées de cache
    (`$XDG_CACHE_HOME`), fixtures de tests, exports/diagnostics. Les formats existants
    en XML (`es_settings.cfg`, `es_systems.cfg`, `es_input.cfg`, `gamelist.xml`, thèmes)
    **restent en XML** pour la compatibilité avec Recalbox/Batocera.
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

## Phase 2 — Hygiène C++23 et outillage (PR #2)

**Objectif** : rendre le code sûr à modifier en masse dans les phases suivantes.

### Fait
1. **Formatage** : `.clang-format` (tabulations, accolades sur nouvelle ligne, largeur 120,
   commentaires non reformatés) et `.clang-format-ignore` (`MameNameMap.cpp`,
   `data/converted/`, `external/`). Code reformaté dans le commit isolé
   `style: apply clang-format…`, listé dans `.git-blame-ignore-revs`
   (`git config blame.ignoreRevsFile .git-blame-ignore-revs`). **Fusionner la PR par
   « merge commit »** (pas « squash ») pour que ce hash reste valide.
2. **Analyse statique** : `.clang-tidy` (bugprone, modernize, performance, quelques
   readability) et cible CMake `es-tidy` (si `run-clang-tidy` est installé). Non bloquant en CI.
3. **Avertissements** : zéro avertissement GCC 14 et Clang 18 dans `es-core`, `es-app`
   et `tests` ; `ES_WARNINGS_AS_ERRORS=ON` en CI et dans le preset `debug`.
   Corrections notables : macro `LOG()` devenue une seule instruction (plus de
   `dangling-else`), **bug réel** de priorité dans `FileData::getCleanName`
   (`a && b || c` déréférençait un système nul), `override` partout, ordre des
   listes d'initialisation, `SliderComponent` ne masque plus `GuiComponent::setValue`.
4. **Modernisations mécaniques** (clang-tidy, relues) : `nullptr`, `using`, `= default`,
   `emplace_back`, littéraux booléens. `modernize-loop-convert` a été **écarté** : il
   nomme les variables de boucle comme des membres (`mCell`), ce qui trompe le lecteur.
5. **Journalisation** : `Log` réécrit (`std::format`, horodatage, mutex, niveaux
   `ERROR/WARN/INFO/DEBUG`). Format de ligne : `YYYY-MM-DD HH:MM:SS.mmm [LEVEL] message`.
   `LOG(level) << …` conservé ; `Log::write(level, "fmt {}", args…)` pour le code nouveau.
   Un message émis avant `Log::open()` part sur `stderr` au lieu d'être perdu.
6. **Tests** : Catch2 v3, option `ES_BUILD_TESTS` (ON dans les presets `debug`,
   `asan`, `relwithdebinfo`, `clang-debug` et en CI), `testPresets`. Les sources de
   `es-app` (sauf `main.cpp`) forment la bibliothèque `es-app-lib` pour être testables.
   13 tests dans `tests/unit/` : `ImageIO` (PNG, WebP, rejet, redimensionnement),
   `Util` (majuscules, `resolvePath`, `makeRelativePath`, `string_to_ptime`),
   `MetaDataList` (valeurs par défaut, **aller-retour XML exact**, chemins relatifs),
   `ThemeData` (chargement d'un thème v3, rejets). Ce sont des **tests de
   caractérisation** : les phases 3 et 4 doivent les garder verts (en adaptant seulement
   les types Boost/Eigen dans les assertions).
7. **Test de fumée** : `tests/smoke/run.sh <binaire> [secondes] [capture.png]` lance ES
   sous Xvfb avec un `HOME` jetable et un système NES, échoue si ES s'arrête seul, si
   aucun log n'est écrit ou si une ligne `[ERROR]` apparaît. En CI, la capture d'écran
   est publiée comme artefact (`smoke-gcc`, `smoke-clang`).

### Reporté (à faire en début de phase 3 par l'agent suivant)
- **Propriété mémoire** (ex-tâche 5) : 93 `new` et 83 `delete` nus restent, dont 19
  `pushGui(new …)`. À traiter **module par module** avec `std::unique_ptr` en documentant
  la propriété (`Window` possède les `GuiComponent` empilés ; les enfants ajoutés par
  `addChild` ne sont **pas** possédés par leur parent). Vérifier chaque étape avec le
  preset `asan` et le test de fumée.
- `enum` → `enum class` (non fait : plusieurs énumérations servent de masques de bits
  ou d'index ; à faire au cas par cas).

### Critères d'acceptation
- Zéro avertissement GCC/Clang dans `es-core` et `es-app` ; `-Werror` actif en CI. ✓
- `ctest --preset debug` vert en CI. ✓
- Aucun changement de comportement visible (seul le format des lignes de log change). ✓

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

Choix faits à l'implémentation (PR #25) :
- `~` (dans `es_systems.cfg`, les thèmes, les gamelists) désigne toujours le vrai `$HOME`,
  même en mode portable ; la migration est sautée en mode portable.
- La migration ne copie pas `es_log.txt` (un nouveau journal suffit) et ne se refait pas
  tant que `~/.emulationstation/MIGRATED_TO_XDG.txt` existe ; un fichier XDG déjà présent
  n'est jamais écrasé.
- `/etc/emulationstation/` (ancien emplacement système d'Aloshi) n'est plus lu ; seuls
  `$XDG_CONFIG_DIRS` et `$XDG_DATA_DIRS` le sont, plus le préfixe d'installation.
- `--home` est lu avant `parseArgs`, car celui-ci instancie `Settings`.

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

### 3.D Propriété mémoire (reliquat de la phase 2)

Jalons livrés, une PR par module :
- #6 `Window` possède les GUI empilés via `unique_ptr`
- #7 `ViewController` : `make_shared`
- #8 `FontFace` : `make_unique`
- #9 `SystemView` : `make_shared`
- #10 `ThemeData` : `make_shared`
- #11 `Scraper` : `make_unique`
- #12 `GamesDBScraper` : `make_unique`
- #13 `InputManager` : tableau d'axes `unique_ptr<int[]>`
- #14 `NinePatchComponent` : tableaux `unique_ptr<T[]>`
- #15 `ComponentGrid` : tableaux `unique_ptr<float[]>`
- #16 `AnimatedImageComponent` : `make_unique`
- #17 `AnimationController` : prend `unique_ptr<Animation>`
- #18 `Settings` : singleton `unique_ptr`
- #19 `SystemData` : `make_unique`
- #20 `TheArchiveScraper` : `make_unique`
- #21 `Gamelist` : `make_unique`
- #22 `Font` : `buildTextCache` `make_unique`
- #23 `GuiComponent` animation + `Sound` buffer : `make_unique`/`unique_ptr`

- #26 **correctif** : `Window::removeGui` supprimait une seconde fois une GUI fermée par
  `delete this` (régression de #6) ; job CI `asan`
- #27 fin de série : `InputConfig` possédés par `InputManager`, `SDL_Event` sur la pile
  (fuite à chaque F4 / « Quitter »), extras de thème, animations de `GuiComponent`
  (`unique_ptr`, `GuiComponent` devient non copiable), `ImageGridComponent` (copiait des
  composants), `ViewController::init`, `Font::buildTextCache` renvoie un `unique_ptr`,
  tampons de `ResourceManager` et `Sound`

Reliquat assumé (voir `PROMPT.md` §0.2) : `new` dans des pointeurs intelligents pour des
constructeurs privés ; arbre `FileData` et `SystemData::sSystemVector` en pointeurs bruts ;
36 `delete this` dans les GUI, qui devraient devenir une fermeture par `Window`.

### État de la phase 3 (audit du 31/10/2026, détails dans [`AUDIT.md`](AUDIT.md))

| Sous-phase | PR | État |
|------------|----|------|
| 3.A Module XDG | #3, #25 | **Fait** : #3 avait livré le module ; #25 y fait passer tous les chemins et ajoute la migration, `--home`, le préfixe d'installation, fontconfig, les tests et l'étape CI. |
| 3.B `std::filesystem` | #4 | Fait. |
| 3.C `std::chrono`, Assign, Variant, Locale | #5 | Fait ; `find_package(Boost)` supprimé. Locale numérique corrigée en 3.E (#28). |
| 3.D Propriété mémoire | #6 à #23, #26, #27 | Fait pour cette phase (voir ci-dessus) ; reliquat listé. |
| 3.E Finitions | #25, #26, #28 | **Fait** : les quatre points. La phase 3 est terminée ; suite : phase 4. |

### 3.E Finitions de la phase 3 (issues de l'audit)

Une PR par point, dans cet ordre :

1. **`modernize/phase-3e-xdg-complete`** — **fait (PR #25, branche `modernize/phase-3a-xdg-complete`)**. Terminer 3.A selon la section 3.A ci-dessus :
   - faire passer par `es::paths` : `Settings.cpp` (`es_settings.cfg`, Config),
     `InputManager.cpp` (`es_input.cfg`, Config), `ThemeData.cpp` (`es_theme_default.xml`,
     Config ; `themes/` via `userDir(Data)` puis `systemDirs(Data)`), `Scraper.cpp`
     (`downloaded_images/`, Data), et l'expansion de `~` (`Util.cpp`, `SystemData.cpp`,
     `ThemeData.cpp`) via `expandUser` ; supprimer `getHomePath()` (et `platform.cpp`
     s'il devient vide) ; supprimer les replis tapés en dur de
     `SystemData::getConfigPath` et `getGamelistPath` (renvoyer le chemin utilisateur) ;
   - appeler `es::paths::migrateFromLegacy()` au démarrage (après `ES_HOME`/`--home`,
     avant `Log::open()` et `Settings`) et la compléter : copie récursive de `themes/`,
     `gamelists/`, `downloaded_images/` ; aucun fichier XDG existant écrasé ; journal de
     chaque copie ; marqueur `MIGRATED_TO_XDG.txt` écrit une seule fois ;
   - `Paths.cpp` : repli `getpwuid`, création des dossiers en `0700`, découpage de
     `XDG_*_DIRS` factorisé, préfixe d'installation par `configure_file`
     (`Paths.config.h.in`, `CMAKE_INSTALL_FULL_DATADIR`/`SYSCONFDIR`) et
     `<exe>/../share/emulationstation` ; formatage `clang-format` du fichier ;
   - option `--home <dossier>` (même effet que `ES_HOME`), affichée dans `--help` ;
   - police de repli via **fontconfig** (`FcFontMatch`, paquet `fontconfig`) ;
   - `tests/unit/test_Paths.cpp` : variables XDG définies/vides/relatives, `ES_HOME`,
     `expandUser`, `find` (utilisateur avant système), migration sur un faux `$HOME` ;
   - étape CI « no-hardcoded-paths » (commande de la section 3.A) ;
   - `tests/smoke/run.sh` : échouer si un fichier est créé hors de `ES_HOME`.
2. **`modernize/phase-3e-numeric-locale`** — **fait (PR #28)**. `setlocale(LC_ALL, "")` suivi de
   `setlocale(LC_NUMERIC, "C")` ; `atof`/`stof` remplacés par `std::from_chars` dans
   `ThemeData.cpp`, `MetaData.cpp`, `RatingComponent.cpp` et tout autre site trouvé par
   `grep -rnE 'atof|atoi|stof|stoi|strtof|sscanf'` ; test unitaire sous
   `fr_FR.UTF-8` (ignoré proprement si la locale n'est pas générée).
3. **`ci/asan`** — **fait (PR #26)**. Ajouter un job CI `asan` (preset `asan`, `ctest`, test de fumée).
   Le retrait de `boost` de la CI a été fait avec l'audit.
4. **`chore/remove-windows-leftovers`** — supprimer `es-app/src/EmulationStation.rc`,
   `EmulationStation.aps`, `data/es_icon.ico` (une icône PNG/SVG arrivera en phase 12)
   et les blocs `#ifdef WIN32`/GLES restants. **Fait (PR #28)**, avec aussi les branches
   `__APPLE__`/`_RPI_` de `VolumeControl` et `Settings`, le réglage `HideConsole` et une
   étape CI qui refuse leur retour.

Critères d'acceptation complets de la phase 3 (**remplis**) : ceux listés plus haut, plus l'étape CI
« no-hardcoded-paths » verte, le job `asan` vert, le test de locale vert, et un
utilisateur qui possède `~/.emulationstation/` retrouve ses systèmes, réglages,
manettes et thèmes au premier lancement.

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

### Découpage en PR
Trois PR : `modernize/phase-4a-math-header` (glm, `Math.h`, tests d'équivalence Eigen/glm,
script de captures), `modernize/phase-4b-glm-core` (migration du code, retrait d'Eigen,
job CI qui compare les captures avec la branche de base), `modernize/phase-4c-glm-app`
(clôture : documentation, paquet `eigen` retiré du job de captures). Inventaire chiffré et détail : section 0.2 de [`PROMPT.md`](../PROMPT.md).

Écart constaté (4a) : `es-core` et `es-app` ne peuvent pas migrer dans deux PR séparées.
Les types traversent l'API (`GuiComponent::getPosition()`, `getSize()`, `render(const
Affine3f&)`, `ThemeData::get<Vector2f>`…) et la syntaxe diffère (`v.x()` contre `v.x`) :
un alias ne peut pas désigner les deux à la fois. La 4b migre donc tout le code d'un
coup, sans alias de transition : le code final écrit directement `glm::vec2`,
`glm::mat4`, et Eigen sort du build dans la même PR.

Références des captures : les artefacts de la CI ne sont pas téléchargeables depuis
l'environnement de l'agent et les rendus varient d'une distribution à l'autre (polices,
Mesa). Plutôt que des PNG committés, le job CI « Screenshots vs base branch » compile la
branche de base et la branche de la PR sur la même machine, prend les captures avec les
deux et les compare (`compare.sh`, tolérance 2 %, 0 pixel). Une PR qui change le rendu
volontairement porte l'étiquette `rendu-modifié`.

### Tâches
1. Ajouter `es-core/src/math/Math.h` qui inclut glm et définit les alias
   `using Vector2f = glm::vec2;` etc. **pour la transition**, puis migrer.
2. Supprimer `EIGEN_DONT_ALIGN` et `Eigen3` de CMake.

### Critères d'acceptation
- [x] Aucune occurrence `Eigen` dans le code, les tests, CMake et la CI (seule la
  documentation en parle encore).
- [x] Rendu identique au pixel près sur les captures : 0 pixel de différence, sans
  tolérance, entre Eigen et glm sur les 6 écrans de `capture.sh` (local, Debug et
  RelWithDebInfo), et job CI « Screenshots vs base branch » vert sur la PR #31.

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

### Découpage en PR
SDL2 et SDL3 ne cohabitent pas dans un même binaire (mêmes noms, signatures différentes) :
comme en phase 4, trois PR.
- `modernize/phase-5a-sdl3-prep` : tests de caractérisation sous SDL2 (`test_Input.cpp` :
  `es_input.cfg`, événements clavier et texte, joystick virtuel branché, utilisé et
  débranché ; `test_Audio.cpp` : conversion WAV et lecture sur le pilote `dummy`), scénario
  de captures étendu (options de liste, saisie de texte dans l'éditeur de métadonnées) et sons
  dans le thème de la fixture.
- `modernize/phase-5b-sdl3` : migration de tout le code vers SDL3 (5.1, 5.2, 5.4, 5.5 et
  joysticks via l'API `SDL_Joystick` de SDL3), SDL2 retiré du build ; mêmes tests.
- `modernize/phase-5c-sdl3-gamepad` : `SDL_Gamepad` (5.3), configuration par défaut des
  manettes reconnues, `gamecontrollerdb.txt`, clôture (documentation, `sdl2-compat` retiré
  du job de captures).

### Écarts constatés
- Les manettes reconnues ne passent pas par les événements `SDL_EVENT_GAMEPAD_*` : ES lit
  toujours les événements joystick bruts, et la configuration par défaut d'une manette
  reconnue traduit chaque bouton standard en l'entrée joystick qui l'alimente
  (`InputManager::mapGamepadDefaults`, via `SDL_GetGamepadBindings`). `InputConfig`,
  `es_input.cfg`, `GuiInputConfig` et les aides restent inchangés ; une entrée de
  `es_input.cfg` passe avant la configuration par défaut. Une manette dont un des dix
  boutons n'a pas de liaison est traitée comme un joystick inconnu (assistant).
- Inversion A/B : réglage `GamepadSwapAB` de `es_settings.cfg`, sans écran pour l'instant
  (le menu des réglages sera repris avec les menus thémés, phase 11).
- Au premier lancement, l'écran de configuration est sauté si une manette reconnue est
  branchée (avant : il fallait un `es_input.cfg`).
- Redimensionnement de la fenêtre (`SDL_EVENT_WINDOW_*`) : non fait, la fenêtre n'est pas
  redimensionnable ; à reprendre avec le moteur de rendu (phase 6).
- Contexte GL laissé en profil compatibilité : le 3.3 core vient avec la phase 6.

### Critères d'acceptation
- [x] Plus aucune référence à `SDL2` dans le code, CMake et la CI (la documentation et les
  commentaires historiques en parlent encore).
- [x] Navigation au clavier : captures scriptées identiques au pixel près entre SDL2 et SDL3
  (8 écrans, RelWithDebInfo et Debug+ASan).
- [x] Manette : vérifiée avec des joysticks et manettes **virtuels** de SDL (axes, boutons,
  chapeau, configuration par défaut, inversion A/B, `es_input.cfg` prioritaire,
  `gamecontrollerdb.txt`), puis par le propriétaire avec une vraie manette (DualShock 4) :
  fonctionne correctement (31/10/2026). Les autres modèles (Xbox, DualSense, 8BitDo) restent
  à essayer.
- [x] Branchement à chaud : joystick et manette virtuels branchés et débranchés pendant
  l'exécution (`test_Input.cpp`).
- [x] Saisie de texte : écran `08-text-input` (« Beta RacerAbc ») en SDL3.
- [x] Sons de navigation : sons du thème de la fixture chargés et joués sur le pilote
  `dummy` à chaque capture ; `test_Audio.cpp` (conversion, lecture, arrêt). Sortie sur une
  vraie carte son non vérifiée ici.

---

## Phase 6 — Moteur de rendu OpenGL 3.3 core

**Objectif** : un rendu moderne, batché, à shaders, prérequis pour la vidéo, les effets de
thèmes (transparence, rotation, dégradés, coins arrondis) et les performances.

Consignes détaillées (inventaire, API, contexte SDL3, shaders GLSL, découpage en PR 6a à
6d, vérifications, pièges) : [`PROMPT.md`, section 13](../PROMPT.md). Elles priment sur
cette section en cas d'écart.

### Conception
> Réalisée (#42, #44, #45) selon `PROMPT.md` 13.3 : `renderer/{Renderer,Renderer_init,
> Shader,Texture,Batcher}.cpp`, `Shaders.h`, `GL.h` (seule inclusion de glad), API sans
> type GL. Ce qui suit est la conception initiale.

- Chargeur de fonctions : **glad 2** généré pour GL 3.3 core (vendored dans
  `external/glad/`, licence MIT/Apache) ; ou `SDL_GL_GetProcAddress` + en-tête maison.
- API interne (`es-core/src/renderer/`) :
  - `Renderer` (singleton existant) garde ses fonctions publiques
    (`pushClipRect`, `popClipRect`, `setMatrix`, `drawRect`) pour limiter l'impact.
  - Nouveau `Renderer::drawTriangles(std::span<const Vertex>, TextureHandle, BlendMode)`.
  - `struct Vertex { glm::vec2 pos; glm::vec2 tex; uint32_t color; };`
  - Un **batcher** accumule les sommets tant que texture/shader/blend/clip ne changent pas ;
    flush sur changement d'état ou en fin d'image.
  - Shaders : `texture_rgba` (images, polices), `color` (rectangles, lignes). Sources GLSL
    `#version 330 core` embarquées en `constexpr std::string_view`. Les shaders vidéo
    `yuv420p` (3 textures `R8`) et `nv12` (VA-API) viennent avec la phase 9.
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
Inventaire à `df6948a` : 118 appels `gl*` dans `Renderer_draw_gl.cpp`,
`Renderer_init_sdlgl.cpp`, `components/{ComponentGrid,ImageComponent,NinePatchComponent}.cpp`,
`resources/{Font,TextureResource}.cpp`, `es-app/src/components/RatingComponent.cpp`,
`es-app/src/main.cpp` ; types GL dans `Renderer.h`, `ImageComponent.h`,
`NinePatchComponent.h`, `Font.h`, `TextureResource.h`, `platform.h` (`GLHEADER`) ;
facteurs de mélange passés par `ComponentList.cpp`. Détail dans `PROMPT.md`, 13.2.

### Découpage en PR
- `modernize/phase-6a-gl-prep` : glad vendored, déplacement vers `es-core/src/renderer/`,
  journalisation de la version GL, tests de la logique pure, captures étendues ; rendu
  identique.
- `modernize/phase-6b-gl33-core` : contexte 3.3 core, shaders, API sans types GL,
  migration de tous les appels ; un envoi par dessin ; rendu identique.
- `modernize/phase-6c-gl-batching` : regroupement des dessins, transformation sur le CPU.
- `modernize/phase-6d-gl-close` : taille de fenêtre, mesure de performance, documentation.

### Critères d'acceptation
- [x] Aucun appel GL hors de `es-core/src/renderer/` : étape CI « No GL outside the
  renderer » (#42).
- [x] Contexte core profile 3.3 obtenu et journalisé (`GL_VERSION` dans le log du test de
  fumée) ; glad core : aucun point d'entrée du pipeline fixe n'est déclaré.
- [x] Aucune erreur `GL_KHR_debug` : sortie active en CI par `ES_GL_DEBUG=1` (test de fumée,
  captures, preset `asan`).
- [ ] Captures : 0 pixel avec la phase 5 pour 6a/6b (#42, comparé à `477f3fd` par #43) ;
  écart du regroupement mesuré dans #44. *À cocher avec les résultats de CI.*
- [x] Retour d'un jeu lancé : capture `09-game-return` (destruction et recréation du
  contexte et des textures).
- [x] Regroupement : ordre conservé (tests du `Batcher`) ; nombre de `glDrawArrays` par
  image donné dans #44 (`draw-calls.txt` des captures).
- [ ] 60 i/s stables sur un iGPU Intel/AMD en 1080p et 4K avec une gamelist de 1 000 jeux :
  **à vérifier par le propriétaire** (procédure ci-dessous).
- [x] `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` traité (viewport et clip mis à l'échelle).

### Mesure de performance (propriétaire)
```bash
cmake --preset release && cmake --build --preset release
tests/smoke/make-big-gamelist.sh ~/es-bench 1000
build/release/emulationstation --home ~/es-bench --resolution 1920 1080 --vsync 0 --draw-framerate
```
Entrer dans la liste « Nintendo Entertainment System » (vue détaillée), garder **Bas**
enfoncé 10 s (défilement rapide), puis **R1/L1** (page par page). Noter le minimum d'i/s
affiché et la ligne `Draws: N (M glDrawArrays)`. Recommencer en 4K
(`--resolution 3840 2160`, plein écran : retirer `--windowed` s'il est configuré). Le
critère est rempli si le minimum reste ≥ 60 i/s dans les deux cas ; donner aussi le modèle
d'iGPU et la ligne `GL_RENDERER` du journal (`~/es-bench/state/emulationstation/es_log.txt`).

### Écarts constatés
- **#40** (un commit, sans le découpage 6a–6d) : couleurs des sommets dans le mauvais ordre
  d'octets, séparateurs de `ComponentGrid` blancs (les 8 captures différaient de 2 000 à
  50 000 pixels), types GL dans les en-têtes publics, pas de glad, `GL_KHR_debug` jamais
  actif en CI, fusionnée avec `rendu-modifié` sans explication. Corrigé et remis aux normes
  par #42 (6a + 6b réunies, rendu de la phase 5 rétabli).
- Le relayout complet quand la taille de la fenêtre change reste hors périmètre : l'image
  est mise à l'échelle, la mise en page garde la taille du lancement. La fenêtre n'est pas
  redimensionnable par l'utilisateur.
- `VolumeControl` (ALSA direct) écrit un `[ERROR]` au retour d'un jeu sans carte son :
  `capture.sh` tolère cette seule ligne (à revoir en phase 8, audio).
- Le tampon de profondeur (`DEPTH_SIZE 16`, `GL_DEPTH_BUFFER_BIT`) est gardé : il ne sert
  pas, mais le retirer change le format de la fenêtre ; à faire avec une mesure.
- `computeClipRect` garde le comportement d'origine : un rectangle qui dépasse son parent
  par le bas ou la gauche est décalé sans être raccourci (caractérisé dans
  `test_Renderer.cpp`).

---

## Phase 7 — Images : formats animés, chargement asynchrone, cache

> « stp » dans la demande initiale désignait bien **stb** (confirmé par le propriétaire).

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

- Scraper ScreenScraper (remplace TheGamesDB legacy et TheArchive, morts), réponses JSON
  analysées avec nlohmann/json.
- Collections automatiques (favoris, derniers joués) et personnalisées.
- Backend audio PipeWire natif.
- Wayland natif vérifié (SDL3 le gère ; tester sous KDE/GNOME/`cage`).
- Rendu via `SDL_GPU` (Vulkan) si OpenGL devient limitant.

---

## Annexe A — Paquets Arch par phase

| Phase | Paquets (`pacman -S --needed`) |
|-------|--------------------------------|
| 1 | `base-devel cmake ninja clang sdl2-compat boost freetype2 eigen curl alsa-lib libglvnd mesa libwebp` |
| 2 | + `catch2 xorg-server-xvfb imagemagick` (tests, test de fumée, capture) |
| 3 | − `boost`, + `fontconfig` (faits) |
| 4 | − `eigen`, + `glm xdotool` (captures scriptées) |
| 5 | − `sdl2-compat`, + `sdl3` |
| 6 | rien : glad est fourni dans `external/glad/` ; `libglvnd mesa` restent |
| 8-9 | + `ffmpeg` (et `libva` pour VA-API) |
| 10 | + `gettext` |
| premier usage de JSON | + `nlohmann-json` |

## Annexe B — Commandes de référence

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug
tests/smoke/run.sh build/debug/emulationstation 6 shot.png
cmake --preset asan && cmake --build --preset asan
cmake --build --preset debug --target es-update-pot   # à partir de la phase 10
./build/debug/emulationstation --windowed --resolution 1280 720 --debug
```
