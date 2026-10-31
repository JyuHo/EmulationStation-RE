# PROMPT — Instructions pour un agent de développement externe

> Ce fichier est le **prompt système de référence** pour tout agent (IA ou humain) qui
> contribue à EmulationStation-RE. Lis-le en entier avant toute action, puis lis
> [`docs/MODERNIZATION_PLAN.md`](docs/MODERNIZATION_PLAN.md). En cas de contradiction,
> l'ordre de priorité est : 1) une instruction explicite du propriétaire du dépôt dans la
> tâche en cours, 2) ce fichier, 3) le plan, 4) tes propres habitudes.

---

## 0. Passation : état du dépôt et point de reprise

> Section à lire en premier. Elle décrit l'état réel du dépôt et dit exactement où
> reprendre. **Mets-la à jour à la fin de chaque phase** (c'est la mémoire du projet entre
> deux agents). Dernière mise à jour : 31/10/2026, fin de la phase 4 (PR #30 à #32, à
> fusionner dans cet ordre). Audit précédent : [`docs/AUDIT.md`](docs/AUDIT.md).

### 0.1 État au moment de la passation
| Phase | État | PR |
|-------|------|----|
| 1 — Build CMake/C++23, CI Arch, FreeImage → stb_image + libwebp | **fusionnée** | #1 |
| 2 — Zéro avertissement, `-Werror`, clang-format/tidy, tests Catch2, test de fumée, Log | **fusionnée** | #2 |
| 3.A — Module XDG `es::paths` | **terminée** : tous les chemins, migration, `--home`, tests, étape CI | #3, #25 |
| 3.B — `std::filesystem` | **fusionnée** | #4 |
| 3.C — `std::chrono`, Assign, Variant, Locale ; Boost supprimé | **fusionnée** (locale numérique corrigée en 3.E) | #5 |
| 3.D — Propriété mémoire (`unique_ptr`) | **terminée** pour cette phase (reste : arbre `FileData`/`SystemData`, `delete this` des GUI) | #6–#23, #26, #27 |
| 3.E — Finitions de la phase 3 (issues de l'audit) | **terminée** : XDG (#25), job `asan` (#26), locale numérique et restes Windows (#28) | #25, #26, #28 |
| 4 — Eigen → glm | **terminée** : glm, `math/Math.h`, captures identiques au pixel près, Eigen retiré ; correctif des chemins `./` des thèmes | #30, #31, #32 |
| **5 — SDL2 → SDL3** | **prochaine étape** (voir 0.2) | — |
| 6 à 12 | à faire | — |

Toutes les PR ont été fusionnées par **merge commit** : le commit de reformatage
`4b943d7` est dans l'historique et `.git-blame-ignore-revs` est valide.

Ce qui existe et que tu dois **réutiliser** (ne le recrée pas) :
- `CMakePresets.json` : configure/build/test presets `debug` (tests + `-Werror`), `release`,
  `relwithdebinfo`, `asan`, `clang-debug`.
- `es-app-lib` : toutes les sources de `es-app` sauf `main.cpp` (cible à lier dans les tests).
- `tests/unit/*.cpp` (Catch2 v3, 42 cas : `ImageIO`, `MetaData`, `ThemeData`, `Paths`,
  `Util`, `Window`, `Locale`, `Math`, `Input`, `Audio`) et `tests/data/` (fixtures). Ajoute tes tests ici,
  un fichier par module (`test_<Module>.cpp`), et déclare-les dans `tests/CMakeLists.txt`.
- `tests/smoke/run.sh <binaire> [secondes] [capture.png]` : test de fumée Xvfb ; il
  utilise `ES_HOME` et un `HOME` jetable, et échoue si un fichier est écrit hors de `ES_HOME`.
- `tests/smoke/capture.sh <binaire> <dossier>` : 9 captures scriptées (xdotool, clavier,
  fixture `tests/data/capture/` : deux systèmes, thème avec logos SVG et sons, liste
  détaillée, menus, options de liste, éditeur de métadonnées, saisie de texte), reproductibles
  au pixel près ; les sons du thème jouent sur le pilote audio `dummy` de SDL ; `tests/smoke/compare.sh <réf> <nouveau> [fuzz]
  [pixels]` les compare. Le job CI « Screenshots vs base branch » le fait pour chaque PR
  (base contre PR, même runner) ; une PR qui change le rendu exprès porte l'étiquette
  `rendu-modifié`. Étends le scénario (`capture.sh`) quand tu touches un écran non couvert.
- `es-core/src/math/Math.h` : types glm (`glm::vec2`, `glm::ivec2`, `glm::mat4`…) et aides
  `getTranslation`, `setTranslation`, `transformPoint`, `roundMatrix`, `roundVector`.
- `es::paths` (`es-core/src/utils/Paths.{h,cpp}`) : **seul** endroit qui connaît des
  chemins. `homeDir`, `userDir` (crée en `0700`), `userFile`, `systemDirs` (XDG, puis
  préfixe d'installation généré par `Paths.config.h.in`, puis `<exe>/../share`), `find`,
  `expandUser`, `setHome`/`hasHomeOverride` (`--home`, `ES_HOME`), `migrateFromLegacy`
  (appelée dans `main` avant toute lecture). Tests : `tests/unit/test_Paths.cpp`.
- Polices de repli CJK via fontconfig (`getFallbackFontPaths` dans `Font.cpp`).
- `Log::write(level, "format {}", args…)` (std::format) pour tout nouveau message ;
  `LOG(level) << …` reste valable dans le code existant. Log dans
  `$XDG_STATE_HOME/emulationstation/es_log.txt`.
- `ImageIO` (stb_image + libwebp) : `loadFromMemoryRGBA32` (lignes de bas en haut) et
  `resizeImageFile`.
- `Util.h` : `applyUserLocale()` (seul appel à `setlocale`), `parseFloat`/`parseInt`
  (`std::from_chars`, valeur de repli) pour tout nombre lu dans un fichier.
- `Window` possède les GUI empilées (`pushGui(std::unique_ptr<GuiComponent>)`).
- `Util.h` : `string_to_ptime` renvoie `std::optional<std::chrono::sys_seconds>` ;
  les dates de `gamelist.xml` (`YYYYMMDDTHHMMSS`) sont traitées comme UTC.
- CI `.github/workflows/build.yml` : Arch (`archlinux:base-devel`), GCC et Clang, build
  `RelWithDebInfo` avec `-Werror`, `ctest`, test de fumée, capture d'écran en artefact.
  Étapes « No hardcoded paths » et « No Windows or embedded leftovers » avant la
  compilation. Job « ASan + UBSan » (preset `asan`, tests et test de fumée, fuites de
  libdbus ignorées par `tests/smoke/lsan.supp`). La locale `fr_FR.UTF-8` est générée
  dans le conteneur pour `test_Locale`. Boost et Eigen ne sont plus installés. Job
  « Screenshots vs base branch » (PR seulement) : captures de la base et de la PR comparées.

### 0.2 Point de reprise : phase 5 (SDL2 → SDL3)
La phase 4 est terminée (PR #30 à #32). Commence la phase 5 en suivant le plan, section
« Phase 5 — Migration SDL2 → SDL3 », la section 9 de ce fichier et
`docs/README-migration.md` du dépôt SDL.

Inventaire à la fin de la phase 4 (refais-le avant de commencer) : 17 fichiers de
`es-core/src` et `es-app/src` utilisent SDL, avec 96 symboles `SDL_*` distincts. Points
lourds : `SDL_JoystickID` (10, `InputManager`, `main.cpp`), audio par rappel
(`SDL_AudioSpec`, `SDL_LockAudio`/`SDL_UnlockAudio`, `SDL_PauseAudio` dans
`AudioManager`/`Sound`), `SDL_GL_SetAttribute` / `SDL_GL_SetSwapInterval`
(`Renderer_init_sdlgl.cpp`), événements clavier et texte (`InputManager`,
`TextEditComponent`). Paquet Arch : `sdl3` remplace `sdl2-compat` (CI, README, annexe A).

Découpage suggéré (à confirmer après lecture du guide de migration) : 5a fenêtre,
contexte GL et événements ; 5b manettes (`SDL_Gamepad`, repli `es_input.cfg`) ; 5c audio.
Si les étapes ne compilent pas séparément (SDL2 et SDL3 ne cohabitent pas dans un même
binaire), fais comme en phase 4 : une PR de préparation (tests, scénario de captures),
une PR de migration, une PR de clôture. Le job de captures ne couvre que le rendu ;
teste les manettes et l'audio à part (`xdotool` ne simule pas de manette).

Reliquat de 3.D, sans série dédiée : traite-le quand une phase réécrit le module.
- `new` restants : uniquement dans des pointeurs intelligents pour des constructeurs privés
  (singletons `InputManager`, `AudioManager`, `ResourceManager`, `VolumeControl`,
  `Settings`, `ViewController`, fabriques de `Font`, `TextureResource`, `Sound`) et
  `AsyncReqComponent` (qui se détruit lui-même).
- Arbre `FileData` (enfants en `FileData*`) et `SystemData::sSystemVector` : propriété
  brute, utilisée partout. À reprendre avec la phase 11 (collections) ou une PR dédiée.
- 36 `delete this` dans les GUI : fonctionnent (voir 0.3) mais devraient devenir
  `mWindow->closeGui(this)` ; à faire avec la refonte des menus thémés (phase 11).

Les tests existants sont des **tests de caractérisation** : ils fixent le comportement
actuel. Tu n'en changes les valeurs attendues que pour un changement de comportement
voulu, expliqué dans la PR.

### 0.3 Pièges déjà rencontrés (ne les redécouvre pas)
- **Paquets Arch** : `freeimage` n'existe plus ; `sdl2-compat` remplace SDL2 (SDL2
  au-dessus de SDL3). Inclus toujours ce que tu utilises : Eigen apportait `<functional>`
  à plusieurs en-têtes sans qu'ils l'incluent, son retrait l'a montré.
- **glm** : `GLM_FORCE_CTOR_INIT` et `GLM_FORCE_EXPLICIT_CTOR` sont des définitions
  publiques d'`es-core` (ne les redéfinis pas ailleurs). Conversion `ivec2` → `vec2` :
  `glm::vec2(v)`. `glm::translate`/`glm::scale` post-multiplient. Point sur un `mat4` :
  `transformPoint`, pas `mat * vec3` (n'existe pas). En phase 6, passe les matrices aux
  uniformes avec `glm::value_ptr` (colonnes, sans transposition).
- **Chemins de thème** : `std::filesystem::path / "/x"` **remplace** le chemin de gauche
  (Boost concaténait). Les chemins `./…` des thèmes en étaient cassés depuis 3.B (corrigé
  en #30) : cherche ce motif dans tout code qui ajoute un suffixe commençant par `/`.
- **UBSan et nanosvg** : `qsort(NULL, 0, …)` dans `external/nanosvg/nanosvgrast.h:787` est
  signalé quand une forme SVG n'a aucune arête (logos de la fixture de captures). Sans
  danger ; à traiter en mettant nanosvg à jour, pas en désactivant UBSan.
- **pugixml vendored ancien** (`external/pugixml`) : pas de `load_string`, utiliser
  `load(const char*)`. Le remplacer par le paquet Arch `pugixml` est possible (phase au
  choix) à condition de garder la compatibilité.
- **stb** : `stb_image_resize2.h` n'est plus publié dans le dépôt stb ; un redimensionnement
  par moyenne de zone est déjà écrit dans `ImageIO.cpp`.
- **clang-format** : le CI n'impose pas le format (les versions de clang-format diffèrent
  entre Arch et les postes). Formate seulement tes lignes modifiées avec
  `git clang-format origin/main`. Ne reformate jamais un fichier entier dans un commit
  qui contient aussi de la logique. Exception : un fichier que tu crées (ex. `Paths.cpp`,
  qui n'a pas été formaté en 3.A et doit l'être).
- **clang-tidy `modernize-loop-convert`** : écarté, il nomme les variables de boucle comme
  des membres (`mCell`). Ne l'applique pas en masse.
- **`std::filesystem` sans `imbue`** : la locale de `boost::filesystem::path` a disparu ;
  les chemins sont des octets UTF-8 (`generic_string()`), ne cherche pas à la recréer.
- **Locale** : la locale de l'utilisateur s'applique par `applyUserLocale()` (`Util.h`), qui
  remet `LC_NUMERIC` à `"C"` : pugixml écrit et lit les flottants avec la bibliothèque C,
  et sous `fr_FR` il écrirait `0,5`. N'appelle jamais `setlocale` ailleurs. Pour lire un
  nombre venant d'un fichier (XML, thème, gamelist), utilise `parseFloat`/`parseInt`
  (`std::from_chars`), jamais `atof`/`stof`/`atoi`.
- **Une seule plateforme** : la CI refuse `WIN32`, `__APPLE__`, `_RPI_`, `USE_OPENGL_ES`
  et `_MSC_VER` dans `es-core/src`, `es-app/src` et `tests`.
- **`ES_HOME` / `--home`** : appliqués par `applyHomeOverride()` au tout début de `main`,
  avant `parseArgs` (qui instancie `Settings`, donc lit `es_settings.cfg`). Tout nouveau
  code qui lit un fichier au démarrage doit venir après. En mode portable, `~` reste le
  vrai `$HOME` et la migration est sautée.
- **Étape CI « No hardcoded paths »** : elle cherche aussi dans les commentaires et les
  messages ; écris `$HOME/…` plutôt que `/home/…` dans un exemple.
- **Propriété des GUI** : `Window` possède les GUI empilées (`pushGui(unique_ptr)`), mais
  elles se ferment encore avec `delete this`. `Window::removeGui` **détache sans
  supprimer** (son appelant est `~GuiComponent`). Ne le change pas sans traiter tous les
  `delete this` : #6 l'avait fait et chaque fermeture de menu libérait deux fois la
  mémoire. `tests/unit/test_Window.cpp` couvre ce cas.
- **`GuiComponent` n'est plus copiable** (il possède ses animations en `unique_ptr`) :
  stocke les composants dans des `unique_ptr` (ex. `ImageGridComponent::mImages`).
- **Animations** : le slot est vidé **avant** la destruction du contrôleur, car son
  `finishedCallback` peut lancer une nouvelle animation dans le même slot ou supprimer le
  composant.
- **Navigation scriptée** : le test de fumée ne fait que démarrer et arrêter ES. Pour
  tester les menus sans écran : `es_input.cfg` clavier dans `ES_HOME`, puis `xdotool key
  F1 Escape …` sous `xvfb-run`, avec le preset `asan`.
- **Fusion des PR** : par **merge commit** (pas squash), sinon le hash de
  `.git-blame-ignore-revs` devient invalide et l'historique des étapes est perdu.
- **Logs** : format `YYYY-MM-DD HH:MM:SS.mmm [LEVEL] message`. Le test de fumée échoue
  sur toute ligne `[ERROR]` : n'utilise `LogError` que pour de vraies erreurs.
- **Lancement de ES sans écran** : sans `es_systems.cfg` valide, ES affiche une erreur et
  quitte ; le test de fumée en fournit un. Au premier lancement, ES affiche l'écran de
  configuration de manette (normal sous Xvfb).

### 0.4 Collaboration avec le propriétaire
- Le propriétaire (GitHub `JyuHo`) relit et **fusionne lui-même** les PR. Ne fusionne pas,
  n'approuve pas, **ne pousse jamais sur `main`**, même pour de la documentation : l'audit
  a relevé deux commits `docs:` poussés directement (`e196fa2`, `a8b6d04`). La mise à jour
  de la documentation fait partie de la PR qui livre le travail.
- Une PR par sous-objectif cohérent, en français, avec les sections décrites en section 6,
  étape 8, et un titre qui décrit **tout** le contenu (pas de « WIP » dans le titre d'une
  PR prête). Ouvre-la en brouillon dès le premier commit pour que la CI tourne, passe-la
  « prête » quand la CI est verte et que la description est complète.
- Évite l'excès inverse : la série 3.D a produit 18 PR de 5 à 30 lignes. Regroupe les
  changements mécaniques d'un même type dans une PR (< ~1 500 lignes).
- En fin de phase : mets à jour la section 0 de ce fichier, coche la phase dans le plan,
  et donne le compte rendu de la section 11. N'annonce une phase « fusionnée » ou
  « terminée » que si tous ses critères d'acceptation sont remplis.

---

## 1. Mission

Tu modernises **EmulationStation-RE**, fork de l'EmulationStation d'Aloshi (v2.0.1a, 2015),
une interface graphique de lancement d'émulateurs pilotable à la manette.

Objectif final :
- C++23, CMake moderne, compilable sans avertissement avec GCC et Clang récents ;
- **SDL3** (fenêtre, entrées, manettes, audio) ;
- rendu **OpenGL 3.3 core** à shaders ;
- images via **stb_image** + **libwebp** (+ nanosvg) ;
- JSON via **nlohmann/json** exclusivement ;
- vidéo et audio via **FFmpeg** ;
- **thèmes sur le modèle de Recalbox 10** ;
- **interface multilingue** via catalogues gettext `.pot`/`.po`/`.mo` (modèle Recalbox / Batocera) ;
- chemins de fichiers **XDG** exclusivement ;
- plateforme **unique** : Linux x86_64, **Arch Linux / CachyOS**.

Tu travailles **phase par phase** dans l'ordre du plan. Tu ne commences jamais une phase
tant que la précédente n'est pas fusionnée (sauf instruction contraire du propriétaire).

---

## 2. Environnement cible

| Élément | Valeur |
|---------|--------|
| OS | Arch Linux / CachyOS, x86_64, à jour |
| Compilateurs | GCC 14+ (référence) et Clang 18+ ; les deux doivent compiler |
| Standard | C++23 (`CMAKE_CXX_STANDARD 23`, extensions désactivées) |
| Build | CMake ≥ 3.25 + Ninja, via `CMakePresets.json` |
| GPU | Mesa (Intel/AMD), OpenGL 3.3 core minimum ; NVIDIA propriétaire toléré |
| Session | X11 et Wayland (SDL3 gère les deux) ; mode kiosque via `cage` |
| CI | GitHub Actions, conteneur `archlinux:base-devel`, matrice GCC/Clang (`.github/workflows/build.yml`) |

Paquets : voir l'annexe A du plan. **Uniquement les dépôts officiels Arch** (`core`, `extra`).
Une bibliothèque absente des dépôts officiels n'est acceptable que si elle est **mono-en-tête
ou très petite**, vendored dans `external/<nom>/` avec son fichier de licence et un
`README.md` indiquant l'URL et le commit d'origine (exemple : `external/stb/README.md`).
Avant d'ajouter une dépendance, vérifie qu'elle existe dans `extra` (la CI le prouvera :
FreeImage a dû être remplacé parce que `pacman` ne le trouvait plus).

---

## 3. Carte du dépôt

```
CMakeLists.txt            racine : options, dépendances (cibles importées), sous-dossiers
CMakePresets.json         presets debug / release / relwithdebinfo / asan / clang-debug
cmake/                    modules CMake du projet (CompilerWarnings.cmake, ...)
es-core/                  bibliothèque statique : rendu, entrées, audio, composants, ressources, thèmes, Settings, Log
  src/components/         composants UI réutilisables (ImageComponent, TextComponent, ComponentList, ...)
  src/guis/               fenêtres génériques (GuiMsgBox, GuiInputConfig, GuiDetectDevice, ...)
  src/resources/          Font, TextureResource, SVGResource, ResourceManager
  src/utils/              Paths (module XDG es::paths)
  src/animations/         animations
es-app/                   exécutable `emulationstation`
  src/                    main.cpp, SystemData, FileData, Gamelist, MetaData, VolumeControl, ...
  src/views/              ViewController, SystemView, gamelist/ (Basic, Detailed, Grid)
  src/guis/               menus (GuiMenu, GuiSettings, GuiMetaDataEd, scrapers, ...)
  src/scrapers/           TheGamesDB legacy / TheArchive (services morts, voir backlog)
data/                     ressources embarquées : resources/ (sources), converted/ (C++ générés), Resources.h
external/                 code tiers vendored : nanosvg, pugixml, stb
tests/                    unit/ (Catch2), data/ (fixtures), smoke/run.sh (test de fumée Xvfb)
.github/workflows/        build.yml (CI Arch : GCC, Clang, ASan + UBSan)
docs/                     MODERNIZATION_PLAN.md, AUDIT.md (+ THEMES.md, I18N.md à créer)
PROMPT.md                 ce fichier
```

Ressources embarquées : `data/converted/*.cpp` sont générés par `res2h` depuis
`data/resources/` (voir `data/generate.sh`). **Ne les modifie pas à la main.** Si tu
ajoutes une ressource, remplace de préférence le mécanisme par `#embed` (C23/C++26, non
disponible partout) ou par une fonction CMake qui génère le tableau d'octets au build ;
sinon régénère avec l'outil et documente-le.

---

## 4. Règles absolues (non négociables)

1. **Chemins XDG, aucun chemin en dur.** Tout accès à un fichier passe par le module
   `es::paths` (`es-core/src/utils/Paths.{h,cpp}`, phase 3.A du plan). Interdits partout
   ailleurs : littéraux `"/etc/…"`, `"/usr/…"`, `"/opt/…"`, `"/home/…"`,
   `"~/.emulationstation"`, `".emulationstation"`, `getenv("HOME")`, concaténations
   `getHomePath() + "/…"`. Les préfixes d'installation viennent de `GNUInstallDirs` via un
   en-tête généré par `configure_file`. Tant que la phase 3.A n'est pas faite, **ne crée
   aucun nouveau chemin en dur** : si une tâche antérieure en a besoin, crée d'abord `Paths`.
   Table XDG à respecter (détails dans le plan) :
   - `$XDG_CONFIG_HOME/emulationstation/` : `es_settings.cfg`, `es_input.cfg`, `es_systems.cfg`, `gamecontrollerdb.txt`
   - `$XDG_DATA_HOME/emulationstation/` : `themes/`, `gamelists/`, `downloaded_images/`, `music/`, `locale/` (surcharges)
   - `$XDG_STATE_HOME/emulationstation/` : `es_log.txt`
   - `$XDG_CACHE_HOME/emulationstation/` : caches
   - `$XDG_CONFIG_DIRS`, `$XDG_DATA_DIRS` (+ préfixe d'installation) : fichiers système, en lecture seule
   Variables vides ou relatives ignorées (spécification XDG). `ES_HOME` / `--home` pour le
   mode portable. Migration depuis `~/.emulationstation/` par **copie**, jamais par
   déplacement ni suppression.
2. **Rien ne casse les données utilisateur.** `es_systems.cfg`, `gamelist.xml`,
   `es_settings.cfg`, `es_input.cfg` existants restent lisibles. Une réécriture de
   `gamelist.xml` doit produire le même contenu si rien n'a changé (test de non-régression).
   Toute conversion de format crée une sauvegarde `.bak` avant d'écrire.
3. **La CI reste verte** (GCC + Clang) à chaque commit poussé. Ne désactive jamais un test,
   un avertissement ou une étape de CI pour obtenir du vert ; corrige la cause.
4. **Pas de copie de code sous licence incompatible.** Le projet est sous licence MIT.
   Recalbox et Batocera sont des **références fonctionnelles** : tu peux lire leur code et
   leur documentation pour comprendre un format (thèmes, organisation des traductions),
   tu ne recopies pas leur code. Code tiers vendored : licence MIT, BSD, zlib, Apache-2.0
   ou domaine public uniquement, fichier de licence inclus.
5. **N'invente pas les formats externes.** Pour tout ce qui touche au format des thèmes
   Recalbox 10, aux conventions de `gamelist.xml` ou aux API de SDL3 / FFmpeg / libwebp,
   vérifie dans la source ou la documentation officielle. Si tu ne peux pas vérifier, écris
   « non vérifié » dans la PR et isole le comportement derrière un point d'extension.
6. **Une seule plateforme.** Ne réintroduis pas de code Windows, macOS, Raspberry Pi
   (`bcm_host`), Mali ou OpenGL ES 1.x. Le code existant sous `#ifdef WIN32` peut être
   supprimé quand tu touches le fichier.
7. **Bibliothèques imposées.** Images : **stb** (+ libwebp pour le WebP). JSON :
   **nlohmann/json** et rien d'autre (paquet Arch `nlohmann-json`, cible CMake
   `nlohmann_json::nlohmann_json` via `find_package(nlohmann_json 3.11 CONFIG REQUIRED)`).
   N'ajoute la dépendance qu'avec son premier usage. Utilise `json::parse(text, nullptr,
   false)` (sans exception) puis teste `is_discarded()` pour les données venant du réseau,
   et `value("clé", défaut)` / `contains()` plutôt que `at()` sur des champs optionnels.
   Pour tes types, définis `to_json`/`from_json` (ou `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`).
   Ne convertis pas les fichiers XML existants (`es_settings.cfg`, `es_systems.cfg`,
   `es_input.cfg`, `gamelist.xml`, thèmes) en JSON : ils restent en XML (pugixml).
8. **Pas d'action irréversible sans accord** : pas de force-push sur `main`, pas de
   suppression de branches d'autrui, pas de publication de paquet ou de release.

---

## 5. Conventions de code

### 5.1 Style
- `.clang-format` fait foi (tabulations, accolades sur une nouvelle ligne, largeur 120).
  Formate uniquement les lignes que tu modifies : `git clang-format origin/main`.
  Les fichiers listés dans `.clang-format-ignore` (générés ou vendored) ne se formatent pas.
- Nommage : `mMember` pour les membres, `sStatic` pour les statiques, `PascalCase` pour les
  types, `camelCase` pour les fonctions, `kConstant` ou `UPPER_CASE` selon le fichier.
- Commentaires : aussi rares que dans le code environnant ; explique le *pourquoi*, pas le *quoi*.
- En-têtes : `#pragma once`. Ordre des inclusions : en-tête associé, en-têtes du projet,
  bibliothèques tierces, bibliothèque standard (ou l'ordre déjà présent dans le fichier).

### 5.2 C++23 attendu dans le code nouveau ou réécrit
- Propriété : `std::unique_ptr` / `std::make_unique` ; `std::shared_ptr` seulement si la
  propriété est réellement partagée (ressources du `ResourceManager`). Aucun `new`/`delete` nu.
- Vues non propriétaires : `std::string_view`, `std::span`.
- Erreurs récupérables : `std::expected<T, Error>` ; exceptions seulement pour l'irrécupérable
  au démarrage. Ne laisse jamais une exception traverser la boucle principale.
- Formatage : `std::format` ; affichage console : `std::print`/`std::println`.
- Énumérations : `enum class` (sauf masques de bits documentés).
- `constexpr`, `[[nodiscard]]` sur les fonctions dont ignorer le résultat est un bug,
  `override` systématique, `final` quand pertinent, `= default` / `= delete` explicites.
- Concurrence : `std::jthread` + `std::stop_token`, `std::mutex` + `std::scoped_lock`,
  `std::atomic`. Les appels OpenGL et la création de textures **uniquement sur le thread
  principal**.
- Interdits : modules C++20 (support outil insuffisant), `using namespace std;` dans un
  en-tête, macros pour des constantes, `std::endl` dans les boucles chaudes.

### 5.3 CMake
- Uniquement des **cibles** : `target_link_libraries`, `target_include_directories`,
  `target_compile_definitions`. Jamais `include_directories`, `link_directories`,
  `add_definitions` ni de variable globale de bibliothèques.
- Dépendances : `find_package(... CONFIG)` si le paquet fournit une config CMake, sinon
  `pkg_check_modules(... IMPORTED_TARGET ...)`. Module `Find*.cmake` maison en dernier recours.
- Toute nouvelle option : préfixe `ES_`, documentée dans le `README.md`.
- Toute nouvelle dépendance : ajoutée à la CI (`pacman -S`), au `README.md` et à
  l'annexe A du plan.

### 5.4 Interface et traduction (à partir de la phase 10, mais anticipe-le)
- Toute chaîne affichée passe par `_()`, `_N()`, `_P()` (voir plan, phase 10).
- Pas de phrases construites par concaténation : un seul message avec des `{}` et
  `std::vformat`. Ajoute un commentaire `// TRANSLATORS: …` quand le contexte n'est pas évident.
- Ne traduis pas : clés de configuration, noms d'éléments XML, messages de log.

---

## 6. Processus pour chaque tâche

1. **Lire** : la section du plan correspondante, puis le code concerné en entier
   (pas seulement les lignes trouvées par `grep`).
2. **Inventorier** : liste les fichiers et sites d'appel touchés
   (`grep -rn` exhaustif) et note-les dans la description de la PR.
3. **Branche** : `modernize/phase-<n><lettre>-<sujet-court>` depuis `main` à jour (ex. `modernize/phase-3a-xdg-paths`).
4. **Implémenter par petits commits** compilables. Après chaque fichier ou groupe cohérent :
   `cmake --build --preset debug`.
5. **Vérifier** (section 8) avec GCC **et** Clang, et avec le preset `asan` dès que tu
   touches à la mémoire, aux threads, à la vidéo ou à l'audio.
6. **Tester** : ajoute ou mets à jour des tests (Catch2, à partir de la phase 2) pour toute
   logique non graphique (chemins, dates, parsing XML, catalogues `.mo`, thèmes).
7. **Documenter** : mets à jour le plan (cocher/ajuster la phase, écarts constatés),
   le `README.md` (dépendances, options) et `docs/THEMES.md` / `docs/I18N.md` si concerné.
8. **PR** : une PR par sous-objectif cohérent, < ~1 500 lignes modifiées hors code généré
   ou reformatage. Description en français avec les sections : *Contexte*, *Changements*,
   *Vérifications effectuées* (commandes + résultats), *Risques / non vérifié*,
   *Suite prévue*.

### Messages de commit
Format Conventional Commits en anglais, impératif, ≤ 72 caractères pour le titre :
`build: …`, `refactor(core): …`, `feat(video): …`, `fix(input): …`, `i18n: …`,
`theme: …`, `docs: …`, `test: …`, `ci: …`, `style: …`. Corps : le *pourquoi* et les
conséquences visibles.

---

## 7. Définition de « terminé » pour une phase

- [ ] Tous les critères d'acceptation de la phase (plan) sont remplis et démontrés.
- [ ] CI verte : GCC et Clang, build Release, tests, test de fumée.
- [ ] Aucun nouvel avertissement (et zéro avertissement à partir de la phase 2).
- [ ] Preset `asan` : lancement + navigation de base sans erreur ASan/UBSan.
- [ ] Étape CI « no-hardcoded-paths » verte (à partir de la phase 3).
- [ ] `README.md`, plan et docs à jour ; dépendances ajoutées à la CI et à l'annexe A.
- [ ] Données utilisateur existantes relues sans perte (fixtures de non-régression).
- [ ] Écarts, limites et points « non vérifiés » listés dans la PR.

---

## 8. Commandes de vérification

```bash
# Dépendances (état après la phase 3 ; adapter selon l'annexe A du plan)
sudo pacman -S --needed base-devel cmake ninja clang sdl2-compat freetype2 fontconfig glm \
    curl alsa-lib libglvnd mesa libwebp catch2 xorg-server-xvfb imagemagick xdotool

# Build GCC et Clang
cmake --preset debug        && cmake --build --preset debug
cmake --preset clang-debug  && cmake --build --preset clang-debug
cmake --preset asan         && cmake --build --preset asan

# Tests unitaires
ctest --preset debug
ctest --preset asan

# Test de fumée sans écran (HOME jetable, un système NES ; affiche le log ; PASS/FAIL)
tests/smoke/run.sh build/debug/emulationstation 6 /tmp/shot.png
# Regarde la capture : c'est le seul contrôle visuel disponible sans écran.

# Captures scriptées (6 écrans : systèmes, liste de jeux, menus) et comparaison
tests/smoke/capture.sh build/debug/emulationstation /tmp/avant
tests/smoke/capture.sh build/debug/emulationstation /tmp/apres
tests/smoke/compare.sh /tmp/avant /tmp/apres     # écart toléré : --fuzz 2 %, 0 pixel

# Analyse statique (non bloquante)
cmake --build --preset debug --target es-tidy

# Chemins en dur et restes d'autres plateformes (mêmes commandes que la CI)
! grep -rnE '"/(etc|usr|opt|home)/|\.emulationstation|getenv\("HOME"\)|getHomePath' \
    es-core/src es-app/src --exclude=Paths.cpp --exclude=Paths.h
! grep -rnE '\b_?WIN32\b|__APPLE__|_RPI_|USE_OPENGL_ES|_MSC_VER' es-core/src es-app/src tests

# Locale française (test_Locale est ignoré si elle manque)
sudo sed -i 's/^#fr_FR.UTF-8/fr_FR.UTF-8/' /etc/locale.gen && sudo locale-gen

# Traductions (à partir de la phase 10)
cmake --build --preset debug --target es-update-pot
```

---

## 9. Indications par phase (complément du plan)

Le plan contient les tâches détaillées ; voici les pièges propres à chaque phase.

**Phase 2 — Hygiène.** Terminée (voir section 0). Si tu appliques encore `clang-tidy
--fix`, fais-le par familles de règles (un commit par famille) et relis chaque diff.

**Phase 3 — Boost et XDG.** Terminée (Boost supprimé, XDG en place, 3.E faite).
Pour XDG, chaque fichier déplacé doit rester trouvable à l'ancien emplacement par la
migration (copie, jamais déplacement) : teste-le sur un faux `$HOME` qui contient un
`~/.emulationstation/` complet. `std::filesystem` lève des exceptions là où le code
Boost n'en attendait pas : utilise les surcharges `std::error_code` dans le parcours des
ROMs et dans `Paths`.

**Phase 4 — Eigen → glm.** Terminée (voir section 0). Les types traversant l'API, la
migration s'est faite en une PR pour tout le code ; le job « Screenshots vs base branch »
a confirmé un rendu identique.

**Phase 5 — SDL3.** Lis `docs/README-migration.md` de SDL3 avant de commencer. Les
fonctions SDL3 renvoient `bool` (true = succès) là où SDL2 renvoyait `0` : chaque test de
retour doit être revu. Les joysticks sont identifiés par `SDL_JoystickID`, pas par index.
`SDL_StartTextInput(window)` est obligatoire pour recevoir du texte. Conserve
`es_input.cfg` comme repli pour les manettes non reconnues par `SDL_Gamepad`.

**Phase 6 — OpenGL 3.3 core.** Aucun appel `gl*` hors de `es-core/src/renderer/`. Active
`GL_KHR_debug` en Debug et traite chaque message. Le batching ne doit pas changer l'ordre
de dessin (transparence). Garde l'API publique de `Renderer` tant que possible.

**Phase 7 — Images.** Décodage hors thread principal, envoi GL sur le thread principal.
Respecte la convention actuelle d'orientation des lignes (de bas en haut) ou change-la
partout en une fois, avec captures de contrôle.

**Phase 8 — Audio.** Un `SDL_AudioStream` par source, liés au même périphérique ; pas de
mixage manuel. La musique se met en pause pendant les vidéos et pendant le jeu.

**Phase 9 — Vidéo.** Horloge maîtresse audio ; files bornées ; arrêt coopératif par
`std::stop_token` ; aucune fuite lors d'une navigation rapide (vérifie avec `asan`).
VA-API optionnel avec repli logiciel automatique et silencieux.

**Phase 10 — Traductions.** Domaine `emulationstation2`. Lecteur `.mo` interne (ne dépend
pas de `locale-gen`) ; la langue vient de `es_settings.cfg` (`Language`). Majuscules
appliquées *après* traduction avec une fonction UTF-8. Livre `fr` complet.

**Phase 11 — Thèmes Recalbox 10.** Étudie d'abord la source et la documentation officielles
de Recalbox (moteur de thèmes de `recalbox-emulationstation` pour la version 10, thème par
défaut) et écris `docs/THEMES.md` **avant** le code : liste exacte des balises, attributs,
vues, éléments, propriétés, mécanismes d'options et de conditions, avec la source de
chaque information. Garde la compatibilité avec le format Aloshi v3. Les erreurs de thème
sont des avertissements localisés (fichier:ligne), jamais un écran noir.

**Phase 12 — Paquet Arch.** `namcap` sans erreur sur le PKGBUILD et le paquet construit.

---

## 10. Ce que tu ne dois pas faire

- Mélanger reformatage et changement de logique dans un même commit.
- Ajouter une dépendance « parce qu'elle est pratique » sans vérifier sa présence dans
  `extra` et sa licence.
- Réécrire un module entier quand une migration incrémentale suffit.
- Supprimer une fonctionnalité utilisateur sans l'écrire dans la PR et dans le plan.
- Laisser du code mort, des `TODO` sans ticket, ou du code commenté.
- Poursuivre une phase si une hypothèse du plan s'avère fausse : arrête-toi, explique
  l'écart et propose une correction du plan.

---

## 11. Compte rendu attendu à la fin de chaque session

En français, court :
1. **Fait** : ce qui est livré (liens vers commits/PR).
2. **Vérifié** : commandes lancées et résultats (copie des lignes significatives).
3. **Non vérifié / risques** : ce qui reste incertain.
4. **Suite** : la prochaine étape concrète.
5. **Questions** : uniquement celles qui bloquent, avec ta recommandation.

---

## 12. Questions ouvertes (à confirmer avec le propriétaire)

- Format exact des thèmes Recalbox 10 et thème de référence à viser pour les tests.
- Liste des langues à livrer en plus de `fr` et `en`.
- Scraper à implémenter en remplacement des services morts (ScreenScraper ?).
