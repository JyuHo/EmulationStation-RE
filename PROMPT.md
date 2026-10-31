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
> deux agents). Dernière mise à jour : 31/10/2026, audit et clôture de la phase 6 (#42,
> #44, #45 après #40) ; mesure de performance faite par le propriétaire (#46). Audit précédent : [`docs/AUDIT.md`](docs/AUDIT.md).

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
| 5 — SDL2 → SDL3 | **terminée** : SDL3 partout, manettes reconnues par `SDL_Gamepad` avec configuration par défaut, captures identiques au pixel près ; testée par le propriétaire avec une vraie manette | #33, #34, #35 (#37) |
| **6 — OpenGL 3.3 core** | **terminée** (≈ 3 500 i/s au minimum sur APU AMD BC-250, 1 000 jeux, mesuré par le propriétaire) : contexte 3.3 core, module `renderer/`, glad, API sans types GL, shaders, `Batcher`, taille de fenêtre ; #40 avait un rendu faux, corrigé par #42 | #40, #42, #44, #45 |
| 7 — Images | **fusionnée** : façade `ImageIO::load` (PNG/JPEG/WebP statique+animé/GIF/AVIF), cache VRAM LRU, `AnimatedImageComponent::loadAnimated` | #41, #47, #48, #49 |
| 8 à 12 | à faire | — |

Toutes les PR ont été fusionnées par **merge commit** : le commit de reformatage
`4b943d7` est dans l'historique et `.git-blame-ignore-revs` est valide.

Ce qui existe et que tu dois **réutiliser** (ne le recrée pas) :
- `CMakePresets.json` : configure/build/test presets `debug` (tests + `-Werror`), `release`,
  `relwithdebinfo`, `asan`, `clang-debug`.
- `es-app-lib` : toutes les sources de `es-app` sauf `main.cpp` (cible à lier dans les tests).
- `tests/unit/*.cpp` (Catch2 v3, 45 cas : `ImageIO`, `MetaData`, `ThemeData`, `Paths`,
  `Util`, `Window`, `Locale`, `Math`, `Input`, `Audio`) et `tests/data/` (fixtures). Ajoute tes tests ici,
  un fichier par module (`test_<Module>.cpp`), et déclare-les dans `tests/CMakeLists.txt`.
- `tests/smoke/run.sh <binaire> [secondes] [capture.png]` : test de fumée Xvfb ; il
  utilise `ES_HOME` et un `HOME` jetable, et échoue si un fichier est écrit hors de `ES_HOME`.
- `tests/smoke/capture.sh <binaire> <dossier>` : 8 captures scriptées (xdotool, clavier,
  fixture `tests/data/capture/` : deux systèmes, thème avec logos SVG et sons, liste
  détaillée, menus, options de liste, saisie de texte dans l'éditeur de métadonnées), reproductibles
  au pixel près ; les sons du thème jouent sur le pilote audio `dummy` de SDL ; `tests/smoke/compare.sh <réf> <nouveau> [fuzz]
  [pixels]` les compare. Le job CI « Screenshots vs base branch » le fait pour chaque PR
  (base contre PR, même runner) ; une PR qui change le rendu exprès porte l'étiquette
  `rendu-modifié`. Étends le scénario (`capture.sh`) quand tu touches un écran non couvert.
- `test_Input.cpp` : `InputFixture` (`ES_HOME` jetable, `InputManager` initialisé, GUI qui
  enregistre ses entrées) et joysticks/manettes **virtuels** SDL (`SDL_AttachVirtualJoystick`)
  pour tester les entrées sans matériel. `test_Audio.cpp` : pilote audio `dummy`
  (`SDL_AUDIO_DRIVER=dummy`, aussi forcé par `capture.sh`).
- Moteur de rendu `es-core/src/renderer/` (phase 6) : **seul** endroit qui appelle `gl*`
  (étape CI « No GL outside the renderer », qui regarde aussi `tests/`, commentaires
  compris : écris « KHR_debug », pas le nom GL complet). API (`Renderer.h`, sans type GL) :
  `Vertex` (`pos`, `tex`, `color` = `convertColor(0xRRGGBBAA)`), `drawRect`,
  `drawTriangles(span<const Vertex>, TextureHandle, BlendMode)`, `drawLines`,
  `createTexture`/`updateTexture`/`destroyTexture` (`RGBA8`, `Alpha8`), `pushClipRect`,
  `setMatrix`, `getDrawCallCount`, `onPixelSizeChanged`. `Batcher` (sans GL, testé) regroupe
  les dessins consécutifs de même état. glad 2 vendored (`external/glad/README.md` pour le
  régénérer, par exemple pour ajouter une extension en phase 9). `ES_GL_DEBUG=1` active
  KHR_debug (exporté par `run.sh` et `capture.sh`). Tests : `tests/unit/test_Renderer.cpp`.
  Script de grande liste : `tests/smoke/make-big-gamelist.sh`.
- SDL3 : `Renderer::getSDLWindow()` (fenêtre, `nullptr` hors `init`/`deinit`) ;
  `AudioManager::lock/unlock` (verrou du flux audio, remplace `SDL_LockAudio`) ;
  `InputManager::mapGamepadDefaults` (configuration par défaut d'une manette reconnue, en
  entrées joystick brutes) ; réglage `GamepadSwapAB` ; `gamecontrollerdb.txt` lu par
  `es::paths::find(Kind::Config, …)` dans `InputManager::init`.
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

### 0.2 Point de reprise : phase 8 (Audio)
**Audit du 31/10/2026.** La PR #40 (un seul commit, sans le découpage 6a–6d) est passée en
3.3 core mais avec les couleurs des sommets dans le mauvais ordre d'octets et les
séparateurs de `ComponentGrid` en blanc : les 8 captures différaient de la phase 5 (2 000 à
50 000 pixels) et elle a été fusionnée avec l'étiquette `rendu-modifié` sans les chiffres ni
la cause demandés en 13.8. Elle gardait aussi les types GL dans les en-têtes publics,
`GL_GLEXT_PROTOTYPES` au lieu de glad, KHR_debug seulement en Debug et un `glGetString`
dans `main.cpp`. #42 a repris 6a et 6b (rendu de la phase 5 rétabli), #44 le regroupement
(6c), #45 la clôture (6d).

La mesure de la phase 6 est faite (propriétaire, 31/10/2026) : environ 3 500 i/s au
minimum en 1080p sur un APU AMD BC-250, mêmes chiffres en 4K (voir « Écarts constatés » du
plan), `Draws: 56 (32 glDrawArrays)`. La phase 7 n'a pas commencé : #41 n'ajoute que la dépendance
`libwebpdemux` (déjà dans `pacman` via `libwebp`). Lis la section « Phase 7 » du plan ; le
chargement asynchrone devra créer les textures sur le thread principal (`createTexture`
ne doit être appelé qu'avec le contexte courant).

Inventaire final de la phase 6 : 0 appel `gl*`, en-tête ou type GL hors de
`es-core/src/renderer/` ; glad n'est inclus que par `renderer/GL.h`.

Restes de la phase 5 (voir le plan, « Écarts constatés ») : d'autres modèles de manettes
(Xbox, DualSense, 8BitDo) restent à essayer ; écran pour `GamepadSwapAB` avec les menus
thémés (phase 11). `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` est traité depuis #45 (image mise
à l'échelle, sans relayout).

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
- **Paquets Arch** : `freeimage` n'existe plus ; SDL2 n'existe plus que sous forme de
  `sdl2-compat` (SDL2 au-dessus de SDL3) ; le projet utilise `sdl3` directement. Inclus toujours ce que tu utilises : Eigen apportait `<functional>`
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
- **SDL3 et `SDL_Quit()`** : `Renderer::deinit` appelle `SDL_Quit()`, qui détruit aussi le
  flux audio et ferme les joysticks. Les singletons qui survivent (l'`AudioManager` est
  détruit à la sortie du programme) doivent vérifier `SDL_WasInit` avant de toucher un
  objet SDL : sinon ES bloque à la sortie (vu en phase 5, détecté par `capture.sh`).
- **SDL3, retours et identifiants** : les fonctions renvoient `bool` (true = succès) ;
  `SDL_EVENT_JOYSTICK_ADDED` donne un `SDL_JoystickID`, pas un index ; SDL3 envoie aussi
  `ADDED` pour les joysticks présents au démarrage (déjà ouverts par `init()`, ignorés).
  `SDL_StartTextInput(window)` est nécessaire pour recevoir du texte (`TextEditComponent`).
- **Pas d'événement texte poussé sous `sdl2-compat`** : `SDL_PushEvent` d'un
  `SDL_TEXTINPUT` plante `sdl2-compat` ; dans les tests, passe l'événement directement à
  `InputManager::parseEvent`.
- **Xvfb** : SDL3 essaie Wayland d'abord et écrit « XDG_RUNTIME_DIR is invalid » sur
  stderr avant de passer à X11 : sans effet. Sans carte son, ouvrir l'audio écrit un
  `[ERROR]` : les scripts forcent `SDL_AUDIO_DRIVER=dummy`.
- **Captures déterministes** : n'ajoute pas d'écran qui affiche un chemin de la fixture
  (copiée dans un `mktemp`) : l'éditeur de métadonnées montre le chemin absolu de l'image.
- **Fusion de PR empilées** : une PR dont la base est une autre branche de PR est
  fusionnée dans cette branche, pas dans `main`. Après la fusion de la PR du dessous,
  repasse la base de la suivante à `main` avant qu'elle soit fusionnée (#35 a dû être
  portée par #37).
- **glad et appels avant `init`** : les pointeurs de fonctions GL sont nuls tant que
  `gladLoadGL` n'a pas tourné. `Window` charge des images dans son constructeur, avant
  `Renderer::init` (et les tests unitaires n'ont jamais de contexte) : les fonctions de
  texture et de dessin ne font rien hors de `init`/`deinit` (`Renderer::isReady`, privé au
  module). Avec libGL (pipeline fixe), ces appels étaient silencieusement ignorés.
- **Ordre des octets des couleurs** : `convertColor(0xRRGGBBAA)` donne les octets R, G, B, A
  en mémoire ; ne copie jamais un `0xRRGGBBAA` tel quel dans `Vertex::color` (erreur de #40).
- **Batcher et état GL** : tout ce qui change l'état d'un dessin en attente (clip, contenu
  ou suppression d'une texture, fin d'image, `deinit`) doit vider le lot avant (`flushBatch`).
- **Retour d'un jeu sans carte son** : `VolumeControl` (ALSA direct) écrit un `[ERROR]` ;
  `capture.sh` ignore cette seule ligne.
- **Job de captures et base fausse** : il compare avec la base de la PR. Pour comparer avec
  un autre état (ici la phase 5, alors que `main` contenait le rendu faux de #40), pousse une
  branche à ce commit et ouvre une PR brouillon « à ne pas fusionner » vers elle (#43).
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
sudo pacman -S --needed base-devel cmake ninja clang sdl3 freetype2 fontconfig glm \
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

**Phase 5 — SDL3.** Terminée (voir section 0). Pour mémoire : lis `docs/README-migration.md`
de SDL3 avant de toucher à SDL. Les
fonctions SDL3 renvoient `bool` (true = succès) là où SDL2 renvoyait `0` : chaque test de
retour doit être revu. Les joysticks sont identifiés par `SDL_JoystickID`, pas par index.
`SDL_StartTextInput(window)` est obligatoire pour recevoir du texte. Conserve
`es_input.cfg` comme repli pour les manettes non reconnues par `SDL_Gamepad`.

**Phase 6 — OpenGL 3.3 core.** Consignes complètes en section 13. Aucun appel `gl*` hors de
`es-core/src/renderer/`. Active `GL_KHR_debug` en Debug (et avec `ES_GL_DEBUG=1`) et traite
chaque message. Le batching ne doit pas changer l'ordre de dessin (transparence). Garde
l'API publique de `Renderer` tant que possible, sans types GL.

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

---

## 13. Phase 6 — Consignes détaillées (moteur de rendu OpenGL 3.3 core)

> **État (31/10/2026)** : réalisée par #42 (6a + 6b, après le rendu faux de #40), #44 (6c) et
> #45 (6d) ; critère de performance vérifié par le propriétaire (APU AMD BC-250).
> Garde cette section comme référence des choix du moteur.

> Cette section est le cahier des charges de la phase 6. Elle complète le plan (section
> « Phase 6 ») et prime sur lui en cas d'écart. Les chiffres viennent de `main` à `df6948a`
> (fin de la phase 5) : **refais l'inventaire** (13.2) avant de commencer et note les
> différences dans la description de ta première PR.

### 13.1 Objectif et périmètre
Remplacer le pipeline fixe d'OpenGL 1.x (matrices `glMatrixMode`/`glOrtho`/`glLoadMatrixf`,
tableaux de sommets clients, `glEnable(GL_TEXTURE_2D)`, `glColor4ub`, texture `GL_ALPHA`)
par un moteur à shaders sur un contexte **OpenGL 3.3 core**, sans changer le rendu.

Dans le périmètre :
- chargeur de fonctions GL (glad 2, vendored) et suppression de `SDL3/SDL_opengl.h` ;
- contexte 3.3 core demandé à SDL3, sortie `GL_KHR_debug` vers le log ;
- module `es-core/src/renderer/` : seul endroit du code qui appelle `gl*` ;
- shaders `#version 330 core` : texture (images, polices) et couleur (rectangles, lignes) ;
- API de `Renderer` sans types GL (plus de `GLenum`, `GLuint`, `GLubyte` hors du module) ;
- textures RGBA8 et atlas de police `GL_R8` ;
- batcher qui regroupe les dessins consécutifs de même état, sans changer leur ordre ;
- prise en compte de `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (reste de la phase 5, voir 13.6) ;
- tests unitaires de la logique sans GL, captures étendues, mesure de performance.

Hors périmètre (ne l'écris pas maintenant, ce serait du code mort, section 10) :
- shaders `yuv420p` et `nv12` : ils arrivent avec la vidéo (phase 9) ;
- stencil, rotation, coins arrondis, dégradés de thème (phase 11) ;
- chargement asynchrone des images, cache VRAM, mipmaps (phase 7) ;
- relayout complet de l'interface quand la taille de la fenêtre change (voir 13.6).

### 13.2 Inventaire du code GL à `df6948a`
Commande (refais-la) :
```bash
grep -rnoE '\bgl[A-Z][A-Za-z0-9]*\s*\(' es-core/src es-app/src tests --include=*.cpp --include=*.h \
    | cut -d: -f1 | sort | uniq -c
grep -rnE 'GLHEADER|GLenum|GLubyte|GLuint|GLint|GL_[A-Z_]+' es-core/src es-app/src tests
```

**118 appels `gl*`, 26 fonctions distinctes, 9 fichiers :**

| Fichier | Appels | Rôle actuel | Ce qui change |
|---------|-------:|-------------|---------------|
| `es-core/src/resources/Font.cpp` | 32 | atlas `GL_ALPHA` (création l. ~297, `glTexSubImage2D` l. ~471 et ~509), dessin d'un `TextCache` (l. ~515-552) | atlas via l'API texture du module (`R8`), dessin via `Renderer::drawTriangles` |
| `es-core/src/Renderer_draw_gl.cpp` | 15 | `pushClipRect`/`popClipRect` (`glScissor`), `drawRect` (tableaux clients `GL_INT`), `setMatrix` (`glLoadMatrixf`), `buildGLColorArray` | réécrit dans `renderer/` |
| `es-core/src/components/ImageComponent.cpp` | 15 | quad texturé, 6 sommets, couleur par sommet (`mColorShift`) | `drawTriangles` |
| `es-core/src/components/NinePatchComponent.cpp` | 15 | 9 quads texturés (54 sommets), deux couleurs | `drawTriangles` |
| `es-app/src/components/RatingComponent.cpp` | 15 | 2 × 6 sommets, deux textures, couleur constante `glColor4ub(255,255,255,opacité)` | `drawTriangles` × 2, couleur par sommet |
| `es-core/src/components/ComponentGrid.cpp` | 10 | séparateurs de cellules en `GL_LINES` | `Renderer::drawLines` |
| `es-core/src/resources/TextureResource.cpp` | 9 | création `GL_RGBA` (MIN `LINEAR`, MAG `NEAREST`, `REPEAT` si tuilée sinon `CLAMP_TO_EDGE`), suppression, `bind()` | API texture du module ; `bind()` disparaît |
| `es-core/src/Renderer_init_sdlgl.cpp` | 6 | `glViewport`, `glMatrixMode`, `glOrtho(0, w, h, 0, -1, 1)`, `glClearColor(1,1,1,1)`, `glClear(COLOR|DEPTH)` dans `swapBuffers` | réécrit dans `renderer/` |
| `es-app/src/main.cpp` | 1 | `glGetString(GL_EXTENSIONS)` (l. ~219) | **plante en core** (voir 13.9) : remplacé par `Renderer` qui journalise version et pilote |

Fréquence : `glEnableClientState`/`glDisableClientState` 15 + 15, `glEnable`/`glDisable`
11 + 11, `glBindTexture` 8, `glDrawArrays` 7, `glVertexPointer` 6, `glBlendFunc` 6,
`glColorPointer` 5, `glTexParameteri`/`f` 4 + 4, `glTexCoordPointer` 4, `glTexSubImage2D`,
`glTexImage2D`, `glScissor`, `glPixelStorei`, `glMatrixMode`, `glGenTextures`,
`glDeleteTextures`, `glColor4ub` 2 chacun, `glViewport`, `glOrtho`, `glLoadMatrixf`,
`glGetString`, `glClearColor`, `glClear` 1 chacun. **Toutes** les fonctions de pipeline fixe
(`glMatrixMode`, `glOrtho`, `glLoadMatrixf`, `gl*Pointer`, `gl*ClientState`, `glColor4ub`,
`glEnable(GL_TEXTURE_2D)`) et le format `GL_ALPHA` n'existent pas en 3.3 core.

Types et en-têtes GL hors des 9 fichiers (à faire disparaître) :
- `GLHEADER` (`platform.h` : `<SDL3/SDL_opengl.h>`) inclus par `Renderer.h`,
  `components/ImageComponent.h`, `resources/{Font,TextureResource}.h` ;
- `GLubyte mColors[6 * 4]` (`ImageComponent.h`), `std::unique_ptr<GLubyte[]> mColors`
  (`NinePatchComponent.h`), `GLuint textureId` / `GLuint* textureIdPtr` /
  `std::vector<GLubyte> colors` (`Font.h`), `GLuint mTextureID` (`TextureResource.h`) ;
- `GLenum` dans la signature de `drawRect` ; deux appelants passent des facteurs de mélange :
  `ComponentList.cpp:218` (`GL_ONE_MINUS_DST_COLOR, GL_ZERO`, inversion) et `:219`
  (`GL_ONE, GL_ONE`, additif). Tous les autres utilisent le défaut
  (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`).

Utilisateurs de l'API publique (elle reste, signatures adaptées) : `Renderer::drawRect`
22 appels dans 11 fichiers, `Renderer::setMatrix` 27 appels, `Renderer::pushClipRect` 6 ;
`buildGLColorArray` 6 appels (ComponentGrid, ImageComponent, NinePatch × 2, Font × 2) ;
`#include "Renderer.h"` dans 36 fichiers. Aujourd'hui, `es-core/src/renderer/` **n'existe
pas** : les fichiers du moteur sont à la racine de `es-core/src`.

Mise à jour ou recréation du contexte : `Window::deinit` (`ResourceManager::unloadAll` puis
`Renderer::deinit`, qui détruit le contexte **et appelle `SDL_Quit`**) et `Window::init`
(`Renderer::init` puis `ResourceManager::reloadAll`) encadrent chaque lancement de jeu
(`SystemData::launchGame`). Tout objet GL du moteur (programmes, VAO, VBO) est donc créé
dans `Renderer::init` et détruit dans `Renderer::deinit`, **avant** `SDL_GL_DestroyContext`.

### 13.3 Architecture cible
Arborescence (nouvelle) :
```
es-core/src/renderer/
  Renderer.h              API publique, sans aucun type ni en-tête GL
  Renderer.cpp            état global, clip, matrices, drawRect/drawTriangles/drawLines, batcher
  Renderer_init.cpp       fenêtre SDL3, contexte 3.3 core, glad, KHR_debug, viewport, swap
  Shader.h / Shader.cpp   compilation et édition de liens, emplacements des uniformes
  Shaders.h               sources GLSL en constexpr std::string_view
  Texture.cpp             création, mise à jour et suppression des textures
  Batcher.h / Batcher.cpp logique de regroupement, sans GL (testable)
  GL.h                    unique inclusion de <glad/gl.h> ; privé au module
external/glad/            glad 2 généré (gl:core=3.3 + GL_KHR_debug), README.md, licences
```
`GL.h` n'est inclus que par les `.cpp` de `renderer/`. Les anciens `Renderer.h`,
`Renderer_draw_gl.cpp`, `Renderer_init_sdlgl.cpp` et la macro `GLHEADER` de `platform.h`
disparaissent. Les 36 `#include "Renderer.h"` deviennent `#include "renderer/Renderer.h"`
(commit mécanique séparé, sans logique).

API publique proposée (adapte les noms si besoin, mais garde l'esprit : aucun type GL) :
```cpp
namespace Renderer {
// 0xRRGGBBAA (convention actuelle du code) -> octets R, G, B, A en mémoire
[[nodiscard]] constexpr std::uint32_t convertColor(unsigned int rgba);

struct Vertex
{
	glm::vec2 pos;
	glm::vec2 tex;
	std::uint32_t color; // résultat de convertColor
};
static_assert(sizeof(Vertex) == 20);

enum class BlendMode { Alpha, Additive, InvertDst }; // (SRC_ALPHA, 1-SRC_ALPHA), (1, 1), (1-DST_COLOR, 0)
enum class TextureFormat { RGBA8, Alpha8 };          // Alpha8 = GL_R8 + swizzle (1, 1, 1, R)
enum class TextureFilter { Nearest, LinearMinNearestMag };
using TextureHandle = unsigned int;                  // 0 = pas de texture

bool init(int w, int h);
void deinit();
unsigned int getScreenWidth();
unsigned int getScreenHeight();
SDL_Window* getSDLWindow();
void swapBuffers();

void pushClipRect(glm::ivec2 pos, glm::ivec2 dim);
void popClipRect();
void setMatrix(const glm::mat4& transform);

void drawRect(float x, float y, float w, float h, unsigned int color, BlendMode blend = BlendMode::Alpha);
void drawRect(int x, int y, int w, int h, unsigned int color, BlendMode blend = BlendMode::Alpha);
void drawTriangles(std::span<const Vertex> vertices, TextureHandle texture, BlendMode blend = BlendMode::Alpha);
void drawLines(std::span<const Vertex> vertices, BlendMode blend = BlendMode::Alpha);

[[nodiscard]] TextureHandle createTexture(TextureFormat format, TextureFilter filter, bool repeat,
	glm::ivec2 size, const void* data); // data peut être nullptr (atlas vide)
void updateTexture(TextureHandle texture, TextureFormat format, glm::ivec2 pos, glm::ivec2 size, const void* data);
void destroyTexture(TextureHandle texture);
} // namespace Renderer
```
- `setMatrix(float*)` est supprimée (aucun appelant hors de `Renderer`).
- `buildGLColorArray` est remplacée par `convertColor` ; les composants stockent des
  `Renderer::Vertex` (supprime leurs `struct Vertex` locales et leurs tableaux de couleurs
  séparés).
- `TextureResource` garde son interface pour le reste du code mais stocke un
  `Renderer::TextureHandle` ; `bind()` est remplacée par un accesseur
  `getTextureHandle()` passé à `drawTriangles`.
- `Font::FontTexture::textureId` et `TextCache::VertexList::textureIdPtr` deviennent des
  `Renderer::TextureHandle` (le pointeur reste : l'identifiant change à chaque
  `deinit`/`reinit`, voir le commentaire dans `Font.h`).

Dessin, état et ordre :
- un seul VAO et un seul VBO de flux (`GL_STREAM_DRAW`, réalloué par `glBufferData(…,
  nullptr, …)` avant chaque envoi), attributs `0 = pos` (2 × float), `1 = tex`
  (2 × float), `2 = color` (4 × `GL_UNSIGNED_BYTE`, **normalisés** : `GL_TRUE`) ;
- `GL_BLEND` toujours actif, `glBlendFunc` selon `BlendMode` (le code actuel l'active et le
  désactive autour de chaque dessin ; les dessins sans mélange n'existent pas) ;
- `GL_SCISSOR_TEST` piloté par la pile de clip, comme aujourd'hui (même calcul, même
  origine en bas à gauche) ;
- projection `glm::ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f)` (équivalente au
  `glOrtho` actuel ; ne définis pas `GLM_FORCE_DEPTH_ZERO_TO_ONE`) et matrice modèle-vue en
  uniformes, passées avec `glm::value_ptr` sans transposition.

### 13.4 Contexte SDL3 et chargeur
Dans `Renderer_init.cpp`, **avant** `SDL_CreateWindow` (les attributs sont lus à la
création de la fenêtre et du contexte) :
```cpp
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, debug ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);
// RED/GREEN/BLUE 8, DOUBLEBUFFER 1, DEPTH 16 : inchangés. Pas d'ALPHA_SIZE (voir 13.9).
```
Après `SDL_GL_CreateContext` (qui rend le contexte courant) :
```cpp
static GLADapiproc loadProc(const char* name)
{
	return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
}
const int version = gladLoadGL(loadProc);
// 0 ou < 3.3 : LogError avec SDL_GetError()/version obtenue, puis return false (pas de repli
// en profil compatibilité).
```
Ensuite : journalise une fois `GL_VERSION`, `GL_RENDERER`, `GL_VENDOR` et
`GL_SHADING_LANGUAGE_VERSION` (`LogInfo`), active `GL_KHR_debug` si demandé, compile les
shaders, crée VAO et VBO, `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` (atlas `R8` de largeur
quelconque), `glViewport`, `glClearColor(1, 1, 1, 1)`.

`GL_KHR_debug` est actif si le build n'a pas `NDEBUG` **ou** si la variable d'environnement
`ES_GL_DEBUG=1` est définie (documente-la dans le `README.md`). `tests/smoke/run.sh` et
`tests/smoke/capture.sh` l'exportent, pour que la CI (`RelWithDebInfo`) en profite. Détails :
`glEnable(GL_DEBUG_OUTPUT)`, `glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS)` (le rappel est alors
appelé sur le thread principal : `Log` n'a pas à être protégé), `glDebugMessageCallback` ;
type `GL_DEBUG_TYPE_ERROR` → `LogError` (le test de fumée échoue alors : c'est voulu),
sévérité `HIGH`/`MEDIUM` → `LogWarning`, le reste → `LogDebug`, `NOTIFICATION` ignorées.
Vérifie `GLAD_GL_KHR_debug` avant (extension, pas cœur de 3.3).

glad 2 :
- génère `gl:core=3.3` avec l'extension `GL_KHR_debug`, en C, **sans** chargeur intégré
  (on passe `SDL_GL_GetProcAddress`). Commande indicative, à vérifier avec `glad --help`
  de la version utilisée : `pip install --user glad2 && glad --api gl:core=3.3
  --extensions GL_KHR_debug --out-path external/glad c` (ou le générateur web
  `https://gen.glad.sh`) ;
- commit `external/glad/{include/glad/gl.h,include/KHR/khrplatform.h,src/gl.c}`,
  `external/glad/README.md` (outil, version, commande exacte, date) et les licences : le
  code généré par glad est dans le domaine public / CC0 / WTFPL et les fichiers Khronos sous
  Apache-2.0 ou MIT (**vérifie** dans le README de glad au moment de la génération et recopie
  la mention) ;
- cible `glad` dans `external/CMakeLists.txt` (bibliothèque statique C, sans les
  avertissements du projet, comme `nanosvg`), liée en `PRIVATE` à `es-core` ;
  `external/*` est déjà dans `.clang-format-ignore` et hors de `es-tidy` ;
- `find_package(OpenGL REQUIRED)` et `OpenGL::GL` restent (libglvnd) ; glad ne lie rien.
- Pourquoi glad plutôt que `SDL_opengl.h` : son en-tête core ne déclare **pas** les fonctions
  retirées. Un `glVertexPointer` oublié devient une erreur de compilation au lieu d'une
  erreur GL à l'exécution.

### 13.5 Shaders
Sources dans `renderer/Shaders.h` (`inline constexpr std::string_view`). Un seul vertex
shader, deux fragment shaders, deux programmes :
```glsl
// vertex (commun)
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
uniform mat4 uModelView;
out vec2 vTex;
out vec4 vColor;
void main()
{
	vTex = aTex;
	vColor = aColor;
	gl_Position = uProjection * uModelView * vec4(aPos, 0.0, 1.0);
}

// fragment "texture" (images, polices)
#version 330 core
in vec2 vTex;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main() { fragColor = vColor * texture(uTexture, vTex); }

// fragment "color" (rectangles, lignes)
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
```
- C'est l'équivalent exact de `GL_MODULATE` (mode d'environnement de texture par défaut
  du pipeline fixe, jamais changé dans le code) : `couleur × texel`.
- Atlas de police : `GL_R8` avec `GL_TEXTURE_SWIZZLE_RGBA = {GL_ONE, GL_ONE, GL_ONE, GL_RED}`.
  Le texel échantillonné vaut `(1, 1, 1, a)`, donc `vColor × texel = (couleur, alpha × a)`,
  exactement ce que donnait `GL_ALPHA` + `GL_MODULATE`. Pas de shader dédié aux polices.
- Erreur de compilation ou d'édition de liens : `glGetShaderInfoLog`/`glGetProgramInfoLog`
  dans un `LogError`, et `Renderer::init` renvoie `false`.
- `uTexture` vaut 0 une fois pour toutes (`glUniform1i` à la création) ; unité de texture 0.

### 13.6 Taille de la fenêtre
Reste de la phase 5. Minimum attendu : sur `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (traité
dans la boucle d'événements de `main.cpp`/`InputManager`), `Renderer` relit
`SDL_GetWindowSizeInPixels`, met à jour `glViewport` et la pile de clip ; la taille logique
(`getScreenWidth/Height`, utilisée partout pour la mise en page) reste celle de
l'initialisation et la projection est conservée, donc l'image est mise à l'échelle. La
fenêtre reste non redimensionnable par l'utilisateur. Le relayout complet est hors
périmètre : écris-le dans « Écarts constatés » du plan.

### 13.7 Découpage en PR
Quatre PR, comme aux phases 4 et 5 (préparation, migration, puis finitions). Chaque PR part
de `main` à jour une fois la précédente fusionnée ; si tu dois empiler, lis 13.10.

**6a — `modernize/phase-6a-gl-prep` (préparation, rendu identique)**
- `external/glad/` + cible CMake, pas encore utilisée par le code (glad et `SDL_opengl.h`
  ne peuvent pas cohabiter dans une même unité de compilation) ;
- déplacement mécanique `Renderer*.{h,cpp}` → `es-core/src/renderer/` et mise à jour des
  36 inclusions (un commit sans logique) ;
- `main.cpp` : remplace la vérification `ARB_texture_non_power_of_two` (cœur depuis GL 2.0)
  par une fonction de `Renderer`, appelée dans `init`, qui journalise `GL_VERSION`,
  `GL_RENDERER` et `GL_VENDOR` avec `glGetString` (valable en compatibilité comme en core) ;
- extraction de la logique pure, testable sans contexte : `convertColor`, intersection des
  rectangles de clip (fonction libre qui prend la pile, le rectangle et la hauteur
  d'écran) ; `tests/unit/test_Renderer.cpp` (cas : couleur `0x11223344` → octets
  `11 22 33 44` en mémoire, clip qui déborde, clip vide, clip imbriqué, `dim` à 0 =
  jusqu'au bord) ;
- correction de `ComponentGrid::updateSeparators` : `mLineColors.reserve(…)` suivi d'une
  écriture par `data()` est un comportement indéfini (la taille reste 0) ; `resize` ;
- `capture.sh` : ajoute un écran qui couvre ce que les 8 actuels ne couvrent pas, sans
  afficher de chemin aléatoire : au minimum **le retour d'un jeu lancé** (la commande de la
  fixture est `true` : `Return` sur un jeu, attente, capture ; cela vérifie la recréation
  du contexte et des textures), et si possible un écran avec séparateurs de
  `ComponentGrid` et étoiles de `RatingComponent` visibles (vérifie dans 03/04 que la note
  est affichée ; sinon ajoute une note à un jeu de la fixture) ;
- `ES_GL_DEBUG=1` exporté par `run.sh` et `capture.sh` (sans effet tant que le contexte est en
  compatibilité, mais prêt) ;
- critère : CI verte, job de captures **sans** l'étiquette, 0 pixel d'écart.

**6b — `modernize/phase-6b-gl33-core` (migration, cœur de la phase)**
- contexte 3.3 core, glad, `GL_KHR_debug` (13.4), shaders (13.5), VAO/VBO ;
- nouvelle API (13.3) ; migration des 9 fichiers et des types GL des en-têtes ; plus aucun
  `GLHEADER`, `SDL_opengl.h` ni type GL hors de `renderer/` ;
- **pas encore de regroupement** : chaque `drawTriangles`/`drawRect`/`drawLines` envoie
  immédiatement (un `glDrawArrays` par appel, comme aujourd'hui) et `setMatrix` met à jour
  l'uniforme. C'est ce qui permet de viser 0 pixel d'écart ;
- critère : CI verte, aucun message `[ERROR]` de `GL_KHR_debug` dans les logs du test de
  fumée et des captures, job de captures sans étiquette. S'il reste un écart, suis 13.8
  avant de poser `rendu-modifié`.
- taille attendue : ~800 à 1 200 lignes hors glad. Si tu dépasses ~1 500, sépare la
  migration des polices (`Font.cpp`) dans une PR intermédiaire, toujours en 3.3 core.

**6c — `modernize/phase-6c-gl-batching` (performance)**
- `Batcher` : accumule les sommets tant que programme, texture, `BlendMode`, primitive
  (triangles ou lignes) et clip ne changent pas ; les sommets sont transformés par la
  matrice modèle-vue **sur le CPU** (`transformPoint`), `uModelView` reste l'identité ;
- flush obligatoire : changement d'état ci-dessus, `pushClipRect`/`popClipRect`,
  `updateTexture` et `destroyTexture` (la texture peut être référencée par des sommets en
  attente), `swapBuffers`, `deinit` ;
- **jamais de réordonnancement** : seuls des dessins consécutifs fusionnent (transparence) ;
- logique séparée du GL : `Batcher` reçoit une fonction d'envoi ; tests dans
  `test_Renderer.cpp` avec un faux envoi (fusion de deux dessins identiques, flush à chaque
  changement d'état, ordre conservé, clip qui force un flush, flush vide sans appel) ;
- compteur de `glDrawArrays` par image, journalisé en `LogDebug` (ou affiché avec
  `DrawFramerate`) : donne le nombre avant/après dans la PR pour les écrans 03 et 05 ;
- la transformation CPU peut déplacer un arrondi : mesure l'écart (13.8).

**6d — `modernize/phase-6d-gl-close` (clôture)**
- `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (13.6) ;
- mesure de performance (13.8, dernier point) et instructions pour le propriétaire ;
- `README.md` (`ES_GL_DEBUG`, GL 3.3 core requis), plan (phase 6 cochée, « Écarts
  constatés »), annexe A (rien à ajouter à `pacman` : glad est vendored ; écris-le), section
  0 de ce fichier (tableau, inventaire final : 0 appel `gl*` hors de `renderer/`, pièges
  rencontrés), compte rendu de la section 11.

### 13.8 Vérifications
À chaque PR, en plus de la section 8 :
```bash
# Aucun appel GL, en-tête ou type GL hors du module (doit ne rien afficher à partir de 6b)
grep -rnE '\bgl[A-Z][A-Za-z0-9]*\s*\(|GLHEADER|SDL_opengl|\bGL(enum|uint|int|ubyte|float)\b|\bGL_[A-Z]' \
    es-core/src es-app/src tests --exclude-dir=renderer

# Contexte réellement obtenu (dans le log du test de fumée)
tests/smoke/run.sh build/debug/emulationstation 6 /tmp/shot.png | grep -E 'GL_VERSION|OpenGL|\[ERROR\]'

# Erreurs GL sous Xvfb, avec le build de CI
ES_GL_DEBUG=1 tests/smoke/capture.sh build/relwithdebinfo/emulationstation /tmp/apres

# Comparaison locale avec main (même machine)
git worktree add /tmp/base origin/main
cmake -S /tmp/base -B /tmp/build-base -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/build-base --target emulationstation
tests/smoke/capture.sh /tmp/build-base/emulationstation /tmp/avant
tests/smoke/compare.sh /tmp/avant /tmp/apres            # attendu : PASS, 0 pixel
tests/smoke/compare.sh /tmp/avant /tmp/apres 2% 200     # si écart : le mesurer

# Mémoire (preset asan) : lancement, captures, retour de jeu
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
tests/smoke/run.sh build/asan/emulationstation 10
```
Ajoute l'étape « No GL outside the renderer » au CI (même commande que la première
ci-dessus, dans le job `arch`, à côté de « No hardcoded paths ») dans la PR 6b.

Captures et étiquette `rendu-modifié` :
1. Le job « Screenshots vs base branch » compare la base et la PR sur le même runner, avec
   `compare.sh` par défaut (fuzz 2 %, **0 pixel**). Il est **sauté** quand la PR porte
   l'étiquette : ne la pose donc jamais avant d'avoir vu le résultat sans elle.
2. Si un écran diffère : télécharge l'artefact `captures-base-vs-head`, regarde les
   `diff-*.png`, mesure avec `compare.sh <base> <head> <fuzz> <pixels>` la plus petite
   tolérance qui passe, et cherche la cause (arrondi des coordonnées, demi-texel, filtre,
   mélange de l'alpha, ordre de dessin). Un écart d'arrondi diffus de quelques pixels est
   acceptable ; une forme décalée, une couleur différente ou un élément manquant ne l'est
   pas.
3. Si l'écart est compris et accepté, écris dans la PR les chiffres par écran (pixels,
   fuzz), la cause et une capture avant/après, puis demande au propriétaire de poser
   `rendu-modifié`. **Cette étiquette n'existe pas encore dans le dépôt** (31/10/2026) :
   le propriétaire la crée ; ne la crée pas toi-même et ne contourne pas le job.
4. La PR suivante repart de `main`, qui contient le nouveau rendu : son job de captures
   doit redevenir à 0 pixel sans étiquette.

Performance (critère du plan, non mesurable en CI : llvmpipe sous Xvfb n'est pas un iGPU) :
- fixture générée par un script (pas 1 000 fichiers commités) :
  `tests/smoke/make-big-gamelist.sh <dossier> 1000` crée les ROMs vides et un
  `gamelist.xml` avec images ;
- mesure avec `DrawFramerate` (affiché par `Window`) et `VSync` désactivé, en 1080p, sur
  la liste détaillée en défilement rapide ; donne le nombre de `glDrawArrays` par image ;
- la validation sur un vrai iGPU Intel/AMD en 1080p et 4K est faite par le **propriétaire** :
  écris la procédure exacte dans la PR 6d (comme pour la manette réelle de la phase 5) et
  ne coche le critère qu'après son retour.

### 13.9 Pièges connus (lis-les avant de coder)
- **`glGetString(GL_EXTENSIONS)` en core** : `GL_INVALID_ENUM` et retour `nullptr`. Le code
  actuel de `main.cpp` construit un `std::string` avec : plantage au démarrage dès le passage
  en core. Supprime-le dans 6a. Pour une extension : `glGetStringi(GL_EXTENSIONS, i)` ou
  les booléens `GLAD_GL_*`.
- **VAO obligatoire** : en core, `glDrawArrays` sans VAO lié donne `GL_INVALID_OPERATION`
  et ne dessine rien (écran blanc, la couleur de fond). Crée et lie le VAO dans `init`.
- **Pas de tableaux clients** : les pointeurs vers la mémoire CPU (`&mVertices[0].pos`)
  sont interdits en core ; tout passe par le VBO.
- **`glEnable(GL_TEXTURE_2D)`** n'existe plus (`GL_INVALID_ENUM`) : supprime-le, ne le
  remplace pas.
- **`GL_ALPHA`/`GL_LUMINANCE`** n'existent plus comme formats : `GL_R8` + `GL_RED` +
  swizzle (13.5). Sans swizzle, le texte apparaît noir ou rouge.
- **Filtre de réduction par défaut** `GL_NEAREST_MIPMAP_LINEAR` : sans mipmaps, une
  texture qui n'a pas son `GL_TEXTURE_MIN_FILTER` réglé est **incomplète** et s'échantillonne
  en noir. Règle toujours MIN et MAG à la création (mêmes valeurs qu'aujourd'hui : polices
  `NEAREST`/`NEAREST`, images MIN `LINEAR` + MAG `NEAREST`).
- **Ordre des octets de couleur** : le code passe des `0xRRGGBBAA` ; l'attribut lit des
  octets R, G, B, A **en mémoire**. Sur x86_64 (petit-boutiste), l'`uint32_t` du sommet vaut
  donc `0xAABBGGRR`. `convertColor` fait la conversion ; teste-la. Ajoute
  `static_assert(std::endian::native == std::endian::little)` (plateforme unique).
- **Attribut couleur non normalisé** : `glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE,
  GL_FALSE, …)` donne des couleurs de 0 à 255 dans le shader, donc tout blanc saturé. Utilise
  `GL_TRUE`.
- **Lignes** : `glLineWidth` > 1 est déprécié en core (erreur sur certains pilotes). Les
  séparateurs font 1 pixel : garde `GL_LINES` à largeur 1 via `drawLines`, ne les convertis
  pas en quads (rendu différent).
- **Couleur constante de `RatingComponent`** : `glColor4ub` n'existe plus et n'est pas
  « collant » en core. Mets `0xFFFFFF00 | opacité` dans les sommets.
- **Recréation du contexte** au lancement d'un jeu (13.2) : programmes, VAO, VBO sont
  perdus avec le contexte. Recrée-les dans `init`, détruis-les dans `deinit` avant
  `SDL_GL_DestroyContext`. L'ordre `ResourceManager::unloadAll` → `Renderer::deinit` →
  `SDL_Quit` doit rester ; un `glDelete*` après la destruction du contexte plante ou fuit.
- **`SDL_Quit` dans `Renderer::deinit`** : ne déplace pas cet appel dans cette phase
  (voir 0.3, `AudioManager`).
- **Attributs SDL avant la fenêtre** : `SDL_GL_SetAttribute` après `SDL_CreateWindow` est
  ignoré pour le profil sur certains pilotes (EGL). Sous Xvfb, SDL3 passe par GLX et Mesa
  (llvmpipe) fournit un contexte core 4.5 : la PR 6a doit le montrer dans le log de CI
  (`GL_VERSION`). Si la CI n'obtient pas de contexte core, arrête-toi et décris l'écart
  (section 10) ; n'ajoute pas de repli en compatibilité.
- **Pas de canal alpha dans la fenêtre** : ne demande pas `SDL_GL_ALPHA_SIZE`. Sous
  Wayland, un tampon avec alpha rend la fenêtre translucide là où l'alpha écrit est < 1 (le
  mélange écrit aussi dans l'alpha).
- **Clip et batcher** : `glScissor` agit au moment du `glDrawArrays`. Un lot construit
  sous un clip et envoyé après `popClipRect` sort du cadre (listes qui débordent). D'où le
  flush avant tout changement de clip.
- **Atlas de police modifié pendant un lot** : `Font` ajoute des glyphes par
  `glTexSubImage2D` quand un `TextCache` est construit, ce qui peut arriver entre deux
  dessins d'une même image ; flush avant `updateTexture`/`destroyTexture`.
- **`IList.h:247`** : `renderTextCache` y compte sur la matrice posée par le rendu de
  `mGradient`. Avec la matrice dans un uniforme ou sur le CPU, conserve ce comportement
  (la dernière matrice posée reste valable jusqu'au prochain `setMatrix`).
- **Coordonnées entières de `drawRect`** : le code arrondit (`round`) les flottants puis
  envoie des `GL_INT`. Garde l'arrondi, envoie des flottants entiers (exacts).
- **Uniformes de matrices** : `glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m))`.
  `GL_TRUE` transpose et déforme tout.
- **`DEPTH_SIZE 16` et `glClear(… | GL_DEPTH_BUFFER_BIT)`** : le tampon de profondeur ne
  sert pas. Garde-les en 6b (rendu identique) ; tu peux les retirer en 6d.
- **`ComponentGrid::mLineColors`** : `reserve` puis écriture par `data()` (UB, voir 6a).
- **Tests sans affichage** : `ctest` tourne en CI **sans** Xvfb. Aucun test unitaire ne
  doit créer de contexte GL : teste la logique pure ; le GL réel est couvert par le test de
  fumée et les captures.
- **`GLM_FORCE_*`** : définitions publiques d'`es-core` (0.3), ne les redéfinis pas dans
  `renderer/`.

### 13.10 Fusion et PR empilées
- Fusion par le propriétaire, par **merge commit** uniquement (jamais squash ni rebase :
  `.git-blame-ignore-revs` et l'historique des étapes en dépendent). Tu ne fusionnes pas.
- Préfère des PR successives : ouvre 6b quand 6a est fusionnée, en partant de `main` à jour.
- Si tu dois empiler (6b ouverte sur la branche de 6a pour avancer) : dans la description,
  écris en tête « Empilée sur #NN : changer la base vers `main` avant de fusionner ». Après
  la fusion de la PR du dessous, **repasse la base de la suivante à `main`** (bouton « Edit »
  du titre de la PR, ou `gh api -X PATCH repos/JyuHo/EmulationStation-RE/pulls/<n> -f
  base=main`) et vérifie que son diff ne contient plus que ses propres commits **avant**
  qu'elle soit fusionnée. Sinon elle se fusionne dans la branche morte et n'atteint pas
  `main` (arrivé à #35, rattrapée par #37).
- Le job de captures compare avec la **base** de la PR : une PR empilée est comparée avec
  la branche du dessous, pas avec `main`. Refais la comparaison après le changement de base.

### 13.11 Critères d'acceptation de la phase 6
- [ ] Aucun appel, en-tête ni type GL hors de `es-core/src/renderer/` (étape CI dédiée).
- [ ] Contexte 3.3 core obtenu, version journalisée ; aucun pipeline fixe restant (glad
  core le garantit à la compilation).
- [ ] Aucun message `GL_DEBUG_TYPE_ERROR` (donc aucun `[ERROR]`) dans le test de fumée, les
  captures et le preset `asan`, avec `ES_GL_DEBUG=1`.
- [ ] Captures : 0 pixel d'écart avec la phase 5 pour 6a et 6b ; pour 6c, écart mesuré,
  expliqué et accepté par le propriétaire (étiquette `rendu-modifié`) ou nul.
- [ ] Retour d'un jeu lancé : capture identique, pas d'erreur ASan.
- [ ] Batching : ordre de dessin conservé (tests), nombre de `glDrawArrays` par image
  donné avant/après.
- [ ] 60 i/s stables en 1080p et 4K avec 1 000 jeux sur iGPU Intel/AMD : **vérifié par le
  propriétaire** selon la procédure de la PR 6d.
- [ ] `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` traité (13.6).
- [ ] Documentation : README, plan, annexe A, section 0 de ce fichier, compte rendu.
