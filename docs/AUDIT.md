# Audit du dépôt — 31 octobre 2026

> Audit de `main` au commit `d2e1339` (fusion de la PR #23), après les phases 1, 2, 3.A,
> 3.B, 3.C et les jalons 3.D. Les constats ont été faits par lecture du code, de
> l'historique git et des exécutions CI ; **le build n'a pas été relancé localement**
> (l'environnement d'audit n'a pas d'Arch Linux ni GCC 14). La CI GitHub Actions est
> verte sur `main` jusqu'à la PR #22 (la PR #23 était en cours d'exécution au moment de l'audit).
>
> **Suivi (31/10/2026, `main` à `8abbd70`)** : les quatre priorités de la section 4 sont
> faites (PR #25, #26, #28) et la phase 3 est terminée. Les constats ci-dessous décrivent
> l'état au moment de l'audit ; l'état courant est dans la section 0 de
> [`PROMPT.md`](../PROMPT.md).
>
> Les actions correctives sont reprises dans [`MODERNIZATION_PLAN.md`](MODERNIZATION_PLAN.md)
> (phase 3.E) et dans la section 0 de [`PROMPT.md`](../PROMPT.md).

## 1. Historique des PR

| PR | Sujet | Fusion | Constat |
|----|-------|--------|---------|
| #1 | Phase 1 : CMake 3.25, C++23, presets, CI Arch, FreeImage → stb_image + libwebp | merge commit | Conforme. |
| #2 | Phase 2 : zéro avertissement, `-Werror`, clang-format/tidy, Catch2, test de fumée, `Log` | merge commit | Conforme. Le commit de reformatage `4b943d7` est bien présent dans l'historique de `main`, donc `.git-blame-ignore-revs` est valide. |
| #3 | 3.A : module `es::paths`, log en `$XDG_STATE_HOME` | merge commit | **Incomplet**, voir §2.1. Annoncé « fusionné/terminé » dans `PROMPT.md` à tort. |
| #4 | 3.B : Boost.Filesystem → `std::filesystem` | merge commit | Conforme (plus aucune occurrence de `boost`). |
| #5 | 3.C : Boost.DateTime/Assign/Variant/Locale → std | merge commit | Fonctionnel, mais introduit un **risque de régression de locale** (§2.2). Titre de PR resté « WIP ». |
| #6 à #23 | 3.D : propriété mémoire, un module par PR | merge commit | Conformes, petites et vertes. 18 PR pour ~300 lignes : granularité excessive mais sans dommage. Le titre de #23 omet le changement de `Sound`. |

Règle de fusion : **toutes** les PR ont été fusionnées par merge commit, comme exigé.

Écarts de processus :
- Deux commits de documentation ont été poussés **directement sur `main`** sans PR
  (`e196fa2 docs: update phase 3.D milestones`, `a8b6d04 docs: close phase 3.D low-hanging ownership`),
  contrairement à `PROMPT.md` §0.4.
- Les branches `modernize/phase-3a-xdg-paths` et `modernize/phase-3b-std-filesystem` existent
  encore sur le dépôt distant alors qu'elles sont fusionnées (à supprimer par le propriétaire).
- Les PR n'ont pas toujours les sections demandées (*Contexte*, *Changements*,
  *Vérifications effectuées*, *Risques / non vérifié*, *Suite prévue*).

## 2. Constats sur le code

### 2.1 Phase 3.A (XDG) inachevée — priorité haute

`es-core/src/utils/Paths.{h,cpp}` existe, mais seuls trois consommateurs l'utilisent :
`Log` (état), `SystemData::getConfigPath` (`es_systems.cfg`) et `SystemData::getGamelistPath`.
Le reste lit et écrit toujours dans `~/.emulationstation` :

| Fichier | Chemin encore en dur |
|---------|----------------------|
| `es-core/src/Settings.cpp:85,106` | `getHomePath() + "/.emulationstation/es_settings.cfg"` |
| `es-core/src/InputManager.cpp:343` | `~/.emulationstation/es_input.cfg` |
| `es-core/src/ThemeData.cpp:338` | `~/.emulationstation/es_theme_default.xml` |
| `es-core/src/ThemeData.cpp:407` | `/etc/emulationstation/themes` et `~/.emulationstation/themes` |
| `es-app/src/scrapers/Scraper.cpp:218` | `~/.emulationstation/downloaded_images/` |
| `es-core/src/resources/Font.cpp:383` | `/usr/share/fonts/truetype/droid/...` (chemin Debian) |
| `es-app/src/SystemData.cpp:403,422` | replis `"/etc/xdg/..."` et `"/usr/share/emulationstation/..."` tapés à la main |
| `es-core/src/platform.cpp`, `Util.cpp:93,167`, `SystemData.cpp:34`, `ThemeData.cpp:107` | `getHomePath()` (expansion de `~`) au lieu de `es::paths::expandUser` |

Conséquences :
- **Régression pour un utilisateur existant** : `es_systems.cfg` est désormais cherché
  uniquement dans les dossiers XDG, et `migrateFromLegacy()` n'est **jamais appelée**.
  Un utilisateur qui avait `~/.emulationstation/es_systems.cfg` voit ES écrire un fichier
  d'exemple dans `~/.config/emulationstation/` puis quitter. Ses réglages et manettes,
  eux, restent lus dans l'ancien dossier : la configuration est éclatée entre deux endroits.
- Les thèmes placés dans `$XDG_DATA_HOME/emulationstation/themes` ne sont pas trouvés.
- `ES_HOME` ne s'applique qu'aux chemins passés par `Paths` : le test de fumée écrit
  encore `es_settings.cfg` et `es_input.cfg` dans le vrai `$HOME/.emulationstation`.

Qualité de `Paths.cpp` par rapport au plan (§3.A) :
- pas de repli `getpwuid(getuid())` si `HOME` est absent (repli sur `/`) ;
- dossiers créés sans permissions `0700` ;
- pas de préfixe d'installation (`configure_file` + `GNUInstallDirs`), ni de recherche
  relative à `/proc/self/exe` : les ressources et thèmes installés ne sont pas trouvés
  hors de `XDG_DATA_DIRS` ;
- `migrateFromLegacy()` ne copie ni `themes/`, ni `gamelists/`, ni `downloaded_images/`,
  ne journalise rien, réécrit `MIGRATED_TO_XDG.txt` à chaque appel et ne vérifie pas que
  les nouveaux dossiers sont vides ;
- logique de découpage de `XDG_*_DIRS` dupliquée ; fichier non formaté selon `.clang-format`
  (accolades, `if` sur une ligne) ; pas de saut de ligne final dans `Paths.h` ;
- option `--home` absente (seule la variable `ES_HOME` existe), non documentée dans `--help` ;
- **aucun test unitaire** de `Paths` (le plan en exige : variables définies/absentes/relatives,
  `ES_HOME`, `expandUser`, migration) ;
- **étape CI « no-hardcoded-paths » absente** de `.github/workflows/build.yml`.

### 2.2 Locale numérique — risque de régression (priorité haute, non vérifié à l'exécution)

`main.cpp` appelle désormais `std::setlocale(LC_ALL, "")` (remplacement de
`boost::locale::generator`). Avec une locale utilisateur française (`LANG=fr_FR.UTF-8`),
`LC_NUMERIC` utilise la virgule décimale ; or `ThemeData.cpp:280` (`atof` des
positions/tailles de thème), `MetaData.cpp:125` (`atof`) et `RatingComponent.cpp:24`
(`stof`) analysent des valeurs écrites avec un point. `atof("0.5")` renverrait alors `0`,
ce qui décale les éléments de thème et met les notes à zéro.
Correctif attendu : ne pas toucher à `LC_NUMERIC` (`setlocale(LC_ALL, "")` puis
`setlocale(LC_NUMERIC, "C")`) **et** remplacer `atof`/`stof` par `std::from_chars`
(indépendant de la locale), avec un test unitaire sous locale `fr_FR`.

### 2.3 Propriété mémoire (3.D)

Les jalons livrés sont propres. Il reste environ 36 `new` et 70 `delete` nus dans
`es-core`/`es-app` (hors `MameNameMap.cpp`), surtout dans les singletons
(`InputManager`, `AudioManager`, `ResourceManager`, `VolumeControl`, `ViewController`),
`TextureResource`, `Font` et `InputConfig`. Le preset `asan` n'apparaît pas dans la CI :
rien ne prouve l'absence de fuite ou de double libération après ces 18 PR.

### 2.4 Divers

- **CI** : installait encore `boost` alors que Boost n'est plus utilisé, si bien que le
  build ne prouvait pas qu'il s'en passe (**retiré dans cette PR**). Pas de job `asan`.
  Pas de vérification de format.
- **Code mort plateforme** : 28 occurrences `WIN32`/`_WIN32`/GLES subsistent ;
  `es-app/src/EmulationStation.rc`, `EmulationStation.aps` (fichier binaire Visual Studio)
  et `data/es_icon.ico` sont des restes Windows.
- **Tests** : 13 cas Catch2 (ImageIO, Util, MetaData, ThemeData). Rien sur `Paths`, ni
  sur l'aller-retour complet d'une `gamelist.xml` exigé par le plan.
- **Installation** : seul le binaire est installé ; pas de ressources, d'exemple
  `es_systems.cfg`, de `.desktop` (prévu en phase 12, mais `Paths` en dépend déjà).
- **README** : était celui d'Aloshi (2015), avec des liens morts (`emulationstation.org`,
  `aloshi.com`), des renvois à `THEMES.md`, `GAMELISTS.md`, `SYSTEMS.md` absents du dépôt,
  et des chemins `~/.emulationstation` devenus en partie faux. **Réécrit dans cette PR.**
- **Documentation** : `PROMPT.md` §0 déclarait la phase 3.A terminée ; dans
  `MODERNIZATION_PLAN.md`, les jalons 3.D étaient placés après le séparateur de la phase 3
  et l'état réel de 3.A/3.B/3.C n'était pas indiqué. **Corrigé dans cette PR.**

## 3. Ce qui est sain

- Build CMake moderne par cibles, presets, refus du build dans l'arbre source.
- CI Arch GCC + Clang, `-Werror`, tests unitaires et test de fumée Xvfb avec capture.
- Boost entièrement supprimé ; dates en `std::chrono` avec tests de format ISO.
- Historique propre : une PR par sujet, fusion par merge commit, `.git-blame-ignore-revs` valide.

## 4. Priorités recommandées

Toutes faites ; la suite est la phase 4.

1. ~~**3.E-1**~~ (PR #25) Terminer XDG : tous les chemins via `es::paths`, appel de la migration, `--home`,
   préfixe d'installation, tests, étape CI « no-hardcoded-paths ».
2. ~~**3.E-2**~~ (PR #28) Corriger la locale numérique (`LC_NUMERIC`, `std::from_chars`) avec test.
3. ~~**3.E-3**~~ (PR #26) CI : ajouter un job `asan` (tests + test de fumée).
4. ~~**3.E-4**~~ (PR #28) Nettoyage : restes Windows, branches fusionnées à supprimer
   (le dépôt n'a plus que `main` comme branche distante).
5. Puis phase 4 (Eigen → glm).
