# PROMPT — Instructions pour un agent de développement externe

> Ce fichier est le **prompt système de référence** pour tout agent (IA ou humain) qui
> contribue à EmulationStation-RE. Lis-le en entier avant toute action, puis lis
> [`docs/MODERNIZATION_PLAN.md`](docs/MODERNIZATION_PLAN.md). En cas de contradiction,
> l'ordre de priorité est : 1) une instruction explicite du propriétaire du dépôt dans la
> tâche en cours, 2) ce fichier, 3) le plan, 4) tes propres habitudes.

---

## 0. Passation : état du dépôt et point de reprise

> Section à lire en premier. Elle décrit l'état laissé par le développeur précédent à la
> fin de la phase 2 et dit exactement où reprendre. **Mets-la à jour à la fin de chaque
> phase** (c'est la mémoire du projet entre deux agents).

### 0.1 État au moment de la passation
| Phase | État | PR |
|-------|------|----|
| 1 — Build CMake/C++23, CI Arch, FreeImage → stb_image + libwebp | **fusionnée** | #1 |
| 2 — Zéro avertissement, `-Werror`, clang-format/tidy, tests Catch2, test de fumée, Log | **terminée** (à fusionner si ce n'est pas fait) | #2 |
| 3 à 12 | à faire | — |

Ce qui existe et que tu dois **réutiliser** (ne le recrée pas) :
- `CMakePresets.json` : configure/build/test presets `debug` (tests + `-Werror`), `release`,
  `relwithdebinfo`, `asan`, `clang-debug`.
- `es-app-lib` : toutes les sources de `es-app` sauf `main.cpp` (cible à lier dans les tests).
- `tests/unit/*.cpp` (Catch2 v3) et `tests/data/` (fixtures). Ajoute tes tests ici, un
  fichier par module (`test_<Module>.cpp`), et déclare-les dans `tests/CMakeLists.txt`.
- `tests/smoke/run.sh <binaire> [secondes] [capture.png]` : test de fumée Xvfb.
- `Log::write(level, "format {}", args…)` (std::format) pour tout nouveau message ;
  `LOG(level) << …` reste valable dans le code existant.
- `ImageIO` (stb_image + libwebp) : `loadFromMemoryRGBA32` (lignes de bas en haut) et
  `resizeImageFile`.
- CI `.github/workflows/build.yml` : Arch (`archlinux:base-devel`), GCC et Clang, build
  `RelWithDebInfo` avec `-Werror`, `ctest`, test de fumée, capture d'écran en artefact.

### 0.2 Point de reprise : phase 3
Fais les PR dans cet ordre (une PR par ligne, chacune verte avant la suivante) :
1. **`modernize/phase-3a-xdg-paths`** — module `es::paths` (plan, section 3.A) et
   remplacement de **tous** les chemins listés dans le plan (section 1.3, point 9),
   migration par copie depuis `~/.emulationstation`, option `--home` / `ES_HOME`,
   étape CI « no-hardcoded-paths », tests unitaires des chemins. Mets à jour
   `tests/smoke/run.sh` pour utiliser `ES_HOME` au lieu de `HOME`, et `Log::getLogPath()`
   pour `$XDG_STATE_HOME`. Le module utilise déjà `std::filesystem`.
2. **`modernize/phase-3b-std-filesystem`** — Boost.Filesystem → `std::filesystem`.
3. **`modernize/phase-3c-std-chrono`** — Boost.DateTime → `std::chrono`, Boost.Assign,
   Boost.Variant, Boost.Locale ; suppression de `find_package(Boost)`.
4. **`modernize/phase-3d-ownership`** — reliquat de la phase 2 : `new`/`delete` nus →
   `std::unique_ptr` (plan, phase 2, « Reporté »).

Les tests existants sont des **tests de caractérisation** : ils fixent le comportement
actuel. Pendant la phase 3, tu ne changes que les types Boost dans leurs assertions
(ex. `boost::posix_time::to_iso_string(...)` → `std::format(...)`), jamais les valeurs
attendues. Si une valeur attendue doit changer, c'est un changement de comportement :
explique-le dans la PR.

### 0.3 Pièges déjà rencontrés (ne les redécouvre pas)
- **Paquets Arch** : `freeimage` n'existe plus ; Arch fournit **Eigen 5** (pas de contrainte
  de version dans `find_package(Eigen3)`, et Eigen 5 n'inclut plus `<cassert>` : inclus
  toujours ce que tu utilises) ; **Boost 1.89** n'a plus de bibliothèque `boost_system` ;
  `sdl2-compat` remplace SDL2 (SDL2 au-dessus de SDL3).
- **pugixml vendored ancien** (`external/pugixml`) : pas de `load_string`, utiliser
  `load(const char*)`. Le remplacer par le paquet Arch `pugixml` est possible (phase au
  choix) à condition de garder la compatibilité.
- **stb** : `stb_image_resize2.h` n'est plus publié dans le dépôt stb ; un redimensionnement
  par moyenne de zone est déjà écrit dans `ImageIO.cpp`.
- **clang-format** : le CI n'impose pas le format (les versions de clang-format diffèrent
  entre Arch et les postes). Formate seulement tes lignes modifiées avec
  `git clang-format origin/main`. Ne reformate jamais un fichier entier dans un commit
  qui contient aussi de la logique.
- **clang-tidy `modernize-loop-convert`** : écarté, il nomme les variables de boucle comme
  des membres (`mCell`). Ne l'applique pas en masse.
- **Fusion des PR** : par **merge commit** (pas squash), sinon le hash de
  `.git-blame-ignore-revs` devient invalide et l'historique des étapes est perdu.
- **Logs** : format `YYYY-MM-DD HH:MM:SS.mmm [LEVEL] message`. Le test de fumée échoue
  sur toute ligne `[ERROR]` : n'utilise `LogError` que pour de vraies erreurs.
- **Lancement de ES sans écran** : sans `es_systems.cfg` valide, ES affiche une erreur et
  quitte ; le test de fumée en fournit un. Au premier lancement, ES affiche l'écran de
  configuration de manette (normal sous Xvfb).

### 0.4 Collaboration avec le propriétaire
- Le propriétaire (GitHub `JyuHo`) relit et **fusionne lui-même** les PR. Ne fusionne pas,
  n'approuve pas, ne pousse pas sur `main`.
- Une PR par sous-objectif, en français, avec les sections décrites en section 6, étape 8. Ouvre-la en
  brouillon dès le premier commit pour que la CI tourne, passe-la « prête » quand la CI est
  verte et que la description est complète.
- En fin de phase : mets à jour la section 0 de ce fichier, coche la phase dans le plan,
  et donne le compte rendu de la section 11.

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
  src/animations/         animations
es-app/                   exécutable `emulationstation`
  src/                    main.cpp, SystemData, FileData, Gamelist, MetaData, VolumeControl, ...
  src/views/              ViewController, SystemView, gamelist/ (Basic, Detailed, Grid)
  src/guis/               menus (GuiMenu, GuiSettings, GuiMetaDataEd, scrapers, ...)
  src/scrapers/           TheGamesDB legacy / TheArchive (services morts, voir backlog)
data/                     ressources embarquées : resources/ (sources), converted/ (C++ générés), Resources.h
external/                 code tiers vendored : nanosvg, pugixml, stb
docs/                     MODERNIZATION_PLAN.md (+ THEMES.md, I18N.md à créer)
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
# Dépendances (état après la phase 2 ; adapter selon l'annexe A du plan)
sudo pacman -S --needed base-devel cmake ninja clang sdl2-compat boost freetype2 eigen \
    curl alsa-lib libglvnd mesa libwebp catch2 xorg-server-xvfb imagemagick

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

# Analyse statique (non bloquante)
cmake --build --preset debug --target es-tidy

# Chemins en dur (à partir de la phase 3)
! grep -rnE '"/(etc|usr|opt|home)/|\.emulationstation|getenv\("HOME"\)' \
    es-core/src es-app/src --exclude=Paths.cpp

# Traductions (à partir de la phase 10)
cmake --build --preset debug --target es-update-pot
```

---

## 9. Indications par phase (complément du plan)

Le plan contient les tâches détaillées ; voici les pièges propres à chaque phase.

**Phase 2 — Hygiène.** Terminée (voir section 0). Si tu appliques encore `clang-tidy
--fix`, fais-le par familles de règles (un commit par famille) et relis chaque diff.

**Phase 3 — Boost et XDG.** Commence par `Paths` (3.A) car il utilise déjà
`std::filesystem`. Les dates des `gamelist.xml` sont au format `YYYYMMDDTHHMMSS` sans
fuseau : traite-les comme UTC et garantis l'aller-retour octet par octet. `std::filesystem`
lève des exceptions là où le code Boost n'en attendait pas : utilise les surcharges
`std::error_code` dans le parcours des ROMs.

**Phase 4 — Eigen → glm.** Fais un commit qui introduit des alias de transition, puis
migre module par module. Attention à `pretranslate`/`prescale` (ordre de multiplication).
Compare des captures d'écran avant/après.

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
