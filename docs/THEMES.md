# Spécification des thèmes — EmulationStation-RE

> Document de référence pour les créateurs de thèmes et pour l'implémentation (phase 11).
> Toute fonctionnalité de thème est décrite ici **avec sa source** avant d'être codée
> (`PROMPT.md` 14.2, R4). Le format 3 d'Aloshi est conservé ; les formats 4 et 5 de Recalbox
> sont ajoutés.
>
> **Sources.** Sauf mention contraire, `chemin:ligne` désigne un fichier du dépôt GitLab
> `recalbox/recalbox`, tag `10.1.1` (commit `a264d628`), sous `projects/frontend/es-core/src/`.
> Ce code est une référence fonctionnelle : il est lu, jamais recopié (R3). Les fichiers de
> ce projet sont écrits avec leur chemin dans le dépôt (`es-core/src/theme/…`).
>
> **État.** Toutes les sections sont vérifiées sur les sources ; ce qui reste à trancher par
> le propriétaire est listé en section 15.

## 1. Emplacements et choix du fichier

- Les thèmes sont dans le dossier `themes/` des dossiers de données (`es::paths`,
  `Kind::Data`, fichier `es-core/src/utils/Paths.h` de ce projet). Le jeu de thèmes actif est le
  réglage `ThemeSet` de `es_settings.cfg`.
- Ordre de recherche du fichier d'un système chez Recalbox (`themes/ThemeManager.cpp:78-85`) :
  1. `<racine du jeu de thèmes>/<dossier du système>/theme.xml` ;
  2. `<racine du jeu de thèmes>/default/theme.xml` (`themes/ThemeManager.h:33`) ;
  3. `<racine du jeu de thèmes>/theme.xml`.
- Ce projet garde en plus le `theme.xml` du dossier des ROM, essayé en premier (comportement
  actuel de `SystemData`).
- Dans ce projet : `ThemeSet::getThemePath` (`es-core/src/ThemeData.h`) applique ces trois
  cas, et `--check-theme` l'appelle. Aucun fichier trouvé : le système n'a pas de thème.

## 2. Versions de format

- La version de format est le contenu de la balise `<formatVersion>` (format 3 d'Aloshi et
  thèmes Recalbox ; `recalbox-next` contient `<formatVersion>4</formatVersion>`).
- Format 3 : lu tel quel, rétrocompatibilité totale (R5).
- Formats 4 et 5 : lus avec les ajouts de Recalbox décrits ici.
- `<formatVersion>` absente ou illisible dans le fichier principal : erreur
  `fichier:0: theme has no <formatVersion>, theme ignored`, le système est affiché sans thème
  (pas d'écran noir, R6). C'est le comportement attendu par `tests/unit/test_ThemeData.cpp`.
- Les nombres sont lus avec `parseFloat`/`parseInt` (`std::from_chars`), jamais avec
  `std::stof` (`PROMPT.md` 0.3).

## 3. Racine `<theme>`

Attributs lus par `ThemeData::FetchCompatibility` (`themes/ThemeData.cpp:984-1036`), qui ne
concernent que la **présentation** du thème dans les menus :

| Attribut | Valeur | Sens | Source |
|---|---|---|---|
| `name` | chaîne | nom affiché ; défaut : nom du dossier | `ThemeData.cpp:1001-1006` |
| `author` | chaîne | auteur ; défaut : vide | `ThemeData.cpp:1008-1013` |
| `version` | `majeur.mineur` ou `majeur` | version **du thème lui-même** (pas du format) | `ThemeData.cpp:1015-1023` |
| `recalbox` | `majeur.mineur` ou `majeur` | version de Recalbox visée | `ThemeData.cpp:1025-1033` |
| `compatibility` | liste parmi `hdmi`, `jamma`, `crt`, `tate` | défaut `hdmi` | `ThemeData.cpp:920-940`, `964-972` |
| `resolutions` | liste parmi `qvga`, `vga`, `hd`, `fhd` | défaut `fhd,hd` | `ThemeData.cpp:942-962`, `974-982` |

Dans ce projet (`es-core/src/theme/ThemeLoader.h`, `ThemeInfo`) : `version` est gardée en
chaîne ; `recalboxVersion` vaut la version **majeure** (`"10"` et `"10.1"` donnent 10).
`compatibility` et `resolutions` sont lues mais n'ont aucun effet (matériel hors périmètre,
`PROMPT.md` 14.1).

## 4. Inclusions

- `<include>` : chemin en texte du nœud ou dans l'attribut `path` ; résolu par rapport au
  fichier qui contient l'`<include>` (`themes/ThemeData.cpp:195-237`).
- Les `<variables>` d'un fichier sont lues, puis ses `<include>`, puis ses `<view>`
  (`ThemeData.cpp:195-197`, `143`).
- Attributs `subset`, `name`, `name.<code>` : inclusion conditionnée par l'option choisie
  (section 8), que le chemin soit dans le texte ou dans `path`.
- Chaque `<include>` a ses conditions `if`, `ifexists`, `ifnotexists` (section 7), évaluées
  avant tout le reste (`ThemeData.cpp:200`).
- Chemin relatif : toujours par rapport au dossier du fichier qui contient l'`<include>`
  (`ThemeData.cpp:211`), jamais par rapport au dossier courant.
- Fichier manquant : avertissement `fichier:ligne`, inclusion ignorée (`ThemeData.cpp:212-216`).
  Cycle ou profondeur excessive : avertissement, inclusion ignorée (décision du projet, Recalbox
  ne détecte pas les cycles).
- Chemin vide après résolution (variable vide, `<include/>`) : avertissement « empty include »,
  inclusion ignorée. Recalbox résout ce chemin vers le dossier du fichier et journalise une
  erreur d'analyse (`ThemeData.cpp:211-227`) ; ici, avertissement seulement.
- Fichier inclus illisible, XML mal formé ou sans `<theme>` : avertissement, inclusion ignorée,
  le reste du thème est chargé. Recalbox journalise une erreur et continue aussi
  (`ThemeData.cpp:225-228`) ; seul le fichier principal peut faire échouer le chargement
  (section 13).
- Un élément redéfini garde la place de sa première déclaration ; ses propriétés fusionnent,
  la dernière valeur gagne (format 3 et Recalbox).

## 5. Variables

- Déclaration : `<variables><variable name="…" value="…"/></variables>`, lue avec ses
  conditions (`themes/ThemeData.cpp:175-193`). Nom ou valeur absent : avertissement.
- Référence : `${nom}`, espaces ignorés ; noms `[A-Za-z_][A-Za-z0-9_]*` avec points simples
  (`themes/ThemeVariablesHolder.cpp`).
- Variables du programme, chez Recalbox (`themes/ThemeManager.cpp`) :
  - globales, `:266-290` : `root`, `recalbox.version`, `recalbox.built`, `settings.language`,
    `settings.locale`, `settings.region`, `settings.timezone`, `hardware.*`, `display.*`
    (dont `display.resolution` = `qvga`/`vga`/`hd`/`fhd`, `:288`) ;
  - fonctions `random.*`, `:308-315` ;
  - système affiché, `:351-366` (`system`, `system.logo`, `system.name`, …) ;
  - jeu et système, `:369-426` (`game.*`).
- Dans ce projet (`ThemeData::loadFile`) :

| Variable | Source Recalbox | Valeur ici |
|---|---|---|
| `root` | `ThemeManager.cpp:270` (racine du jeu de thèmes) | dossier au-dessus de `<jeu>/<système>/theme.xml` ou `<jeu>/default/theme.xml`, sinon dossier du fichier |
| `system.name` | `ThemeManager.cpp:357` (nom court) | `<name>` de `es_systems.cfg` ; absente hors d'un système |
| `system` | `ThemeManager.cpp:355` (nom complet) | `<fullname>` de `es_systems.cfg` ; absente hors d'un système |
| autres (`recalbox.*`, `settings.*`, `hardware.*`, `display.*`, `random.*`, `system.*` restantes, `game.*`) | `ThemeManager.cpp:266-426` | non prises en charge : non définies, `${…}` laissé tel quel (`ThemeVariables::resolve`) |

## 6. Substitutions historiques dans les chemins

`ThemeData::resolveSystemVariable` (`themes/ThemeData.cpp:714-748`), appliquée aux chemins des
`<include>` et des propriétés :

- `$system` : dossier du système ; si le chemin obtenu n'existe pas, essai avec `default`
  (`:718-730`) ;
- `$language`, `$country` : codes de la langue active (`:731-732`) ;
- avec un système : `$fullname`, `$type`, `$pad`, `$keyboard`, `$mouse`, `$releaseyear`,
  `$netplay`, `$lightgun` (`:734-744`) ;
- `$random(…)` : un chemin tiré au hasard dans la liste, le même pour tout le thème
  (`:750-767`).

Dans ce projet (`applyLegacySubstitutions`, `es-core/src/theme/ThemeVariables.h`), appliquée
aux `<include>`, aux `ifexists`/`ifnotexists` et à toutes les propriétés :

| Substitution | Valeur ici |
|---|---|
| `$system` | `<theme>` de `es_systems.cfg`, repli sur `default` comme chez Recalbox ; vide hors d'un système |
| `$language`, `$country` | réglage `Language` : `fr_FR` donne `fr` et `FR`, `en` donne `en` et une chaîne vide |
| `$fullname`, `$type`, `$pad`, `$keyboard`, `$mouse`, `$releaseyear`, `$netplay`, `$lightgun`, `$random(…)` | non prises en charge : laissées telles quelles (ce projet n'a pas ces données) |

## 7. Conditions

- Attributs `if`, `ifexists`, `ifnotexists` sur tout nœud (`themes/ThemeData.cpp:782-810`).
  Condition vide : considérée vraie, avec avertissement.
- Opérateurs `!` `not` `&` `and` `|` `or` `(` `)`, évalués de **gauche à droite, sans
  priorité** (`ThemeData.cpp:812-885`, `utils/eval/SimpleTokenizer.cpp`). Grammaire de ce
  projet : `es-core/src/theme/ThemeCondition.h`.
- Identifiants (`ThemeData::EvaluateIdentifier`, `themes/ThemeData.cpp:887-919`) et valeur
  dans ce projet (`makeConditionResolver`, `es-core/src/theme/ThemeCondition.cpp`) :

| Identifiant | Sens chez Recalbox | Valeur ici |
|---|---|---|
| `qvga` | hauteur d'affichage ≤ 288 (`Renderer.h:720`) | idem, hauteur réelle de l'écran |
| `vga` | 288 < hauteur ≤ 576 (`Renderer.h:722`) | idem |
| `hd` | 576 < hauteur ≤ 920 (`Renderer.h:724`) | idem |
| `fhd` | hauteur > 920 (`Renderer.h:726`) | idem |
| `ispc` | matériel PC | `true` |
| `crt`, `jamma`, `overscan`, `tate`, `tateright`, `tateleft`, `bartop`, `nomenu` | matériel ou mode Recalbox | `false` (hors périmètre) |
| `ispi`, `isodroid`, `isanbernic` | carte | `false` |
| `virtual`, `arcade`, `port` | nature du système | `false` (ce projet n'a pas cette donnée) |
| `console`, `handheld`, `computer`, `fantasy`, `engine` | type du système | `false` (ce projet ne connaît pas le type) |
| `favorite`, `lastplayed` | système virtuel des favoris / derniers joués | `false` |
| autre | inconnu : faux, message de débogage | avertissement `fichier:ligne`, faux |

Avant `Renderer::init` (par exemple `--check-theme`), la taille supposée est 1920 × 1080, ou
celle de `--resolution`.

## 8. Options (subsets)

- `<subset subset="…" title="…" help="…" title.<code>="…" help.<code>="…"/>` déclare une
  option et son texte (`themes/ThemeData.cpp:153-173`).
- `<include subset="…" name="…" name.<code>="…">` est une valeur possible de cette option.
- Option sans choix enregistré, ou choix disparu : première valeur disponible (décision du
  projet, `PROMPT.md` 14.5).
- Enregistrement : clé `ThemeOption.<jeu de thèmes>.<subset>` (chaîne) dans `es_settings.cfg`
  (décision du projet, `PROMPT.md` 14.5), lue sans `[ERROR]` si absente.
- Menu « Options du thème » : PR 11e ; choix appliqué au chargement depuis 11e2
  (`ThemeData::loadFile`, test `[11e2]` de `tests/unit/test_ThemeData.cpp`).

## 9. Régions et langues

- Attribut `region` (liste de codes à deux lettres, séparés par des virgules) : le nœud n'est
  gardé que si un code correspond à la région du thème (`themes/ThemeData.cpp:360-370`).
  Régions de Recalbox : `us`, `eu`, `jp` (`ThemeData.cpp:107-110`) ; ce projet : réglage
  `ThemeRegion`, défaut `eu` (`PROMPT.md` 14.5).
- Suffixe localisé `.<code>` sur un nom d'attribut ou de propriété (`path.fr`, `text.fr_FR`) :
  `ExtractLocalizedCode` (`themes/ThemeData.cpp:769-780`) lit 2 lettres (langue) ou 5
  (langue et pays). Une valeur localisée qui correspond gagne sur la valeur sans suffixe, quel
  que soit l'ordre de déclaration.
- Codes comparés : langue, langue et pays, région (`ThemeData.cpp:165-166`).

## 10. Vues

Vues lues par Recalbox (`ThemeSupport::SupportedViews`, `themes/ThemeSupport.cpp:155-164`) :
`system`, `basic`, `detailed`, `menu`, `gameclip`, `challenge`.

| Vue | Statut ici |
|---|---|
| `system` | lue (format 3) ; carrousel et nouveautés : PR 11g |
| `basic`, `detailed` | lues (format 3) |
| `menu` | lue dans `<jeu>/theme.xml` seul, sans système (`ThemeManager.cpp:54-74`) depuis 11f1 ; application aux menus : 11f2 (partielle), 11f4, 11f5 |
| `gameclip` | économiseur d'écran : PR 11h |
| `gamelistlogo` | non prise en charge. **Contradiction** chez Recalbox : testée par `ThemeData::HasLogoView` (`themes/ThemeData.h:93`) mais absente de `SupportedViews` ; à trancher par le propriétaire (R13) |
| `challenge` | non prise en charge (pas de système de défis) : avertissement, vue ignorée |

### Vue menu (11f1)

- Fichier : `<jeu de thèmes>/theme.xml` seul, chargé sans système ; pas de repli sur
  `default/theme.xml` (`ThemeManager.cpp:54-74`). Absent ou invalide : avertissement, menus
  par défaut (`ThemeData::loadMainTheme`).
- Éléments lus par leur nom (`MenuThemeData.cpp:4-130`) : `menuSize` « menuSize »,
  `menuBackground` « menubg », `menuText` « menutitle », « menufooter », « menutext »,
  `menuTextSmall` « menutextsmall », `menuSection` « menusection », `menuButton`
  « menubutton », `menuSwitch` « menuswitch », `menuSlider` « menuslider », `menuIcons`
  « menuicons ». Toute valeur est facultative : absente, le composant garde la sienne.
- `menusection` part des valeurs de `menutext` (`MenuThemeData.cpp:49`) ; police par défaut :
  celle de `menutext`.
- `menutextsmall` part de `menutext` (`:65`) ; ses `selectedColor` et `selectorColor`
  écrivent la couleur sélectionnée de `menutext` (`:72-73`, reproduit tel quel).
- `keyboard` : lu dans la vue `menu`, sinon `system` (`WindowManager.cpp:131-145`).
- Tests : `[11f1]` de `tests/unit/test_MenuTheme.cpp`.

### Propriétés communes et ordre de dessin (11d)

Recalbox applique ces propriétés dans `components/base/Themable.cpp:66-77` (sources de
l'interface, même version que `themes/`) ; ce projet fait de même dans
`GuiComponent::applyCommonTheme`.

- `zIndex` : les enfants d'une vue sont dessinés du plus petit au plus grand, tri **stable**
  (`components/base/Component.cpp:176-180`) : à valeur égale, l'ordre de déclaration reste.
  Sans `zIndex`, l'élément prend la valeur par défaut de son composant (tableau ci-dessous).
- `rotation` : degrés, sens horaire, autour de `rotationOrigin` (fractions de la taille,
  depuis le coin haut gauche). Sans `rotation` : 0. Sans `rotationOrigin` : `0 0`, le coin
  haut gauche (`Themable.cpp:69`) ; la documentation de Recalbox annonce `0.5 0.5` : le code
  fait foi (décision du propriétaire, 01/11/2026).
- `disabled` : `true` = l'élément n'est pas dessiné. Lu avec la position (`Themable.cpp:76-77`).

| Vue | Élément | `zIndex` par défaut | Source Recalbox |
|---|---|---|---|
| `basic`, `detailed` | `background` | 0 | valeur initiale, `Component.cpp:22` |
| `basic`, `detailed` | éléments `extra` | 10 | `themes/ThemeData.cpp:605` |
| `basic`, `detailed` | `gamelist` | 20 | `views/gamelist/DetailedGameListView.cpp:82` |
| `detailed` | `md_image`, `md_video` | 30 | `DetailedGameListView.cpp:94`, `:112` |
| `detailed` | étiquettes et valeurs `md_*`, `md_description` | 40 | `DetailedGameListView.cpp:150`, `:349`, `:377` |
| `basic`, `detailed` | `logo`, `logoText` | 50 | `views/gamelist/ISimpleGameListView.cpp:32`, `:37` |
| `system` | éléments `extra` | 10, triés entre eux | `themes/ThemeData.cpp:605` |

Ces valeurs suivent l'ordre où ce projet dessinait déjà ces éléments : un thème format 3, sans
`zIndex`, garde exactement son rendu (R5). Les éléments `extra` d'une vue de liste sont
désormais des enfants de la vue (`ThemeExtras`, paramètre `host`) pour se placer entre les
autres éléments. Vue `system` : le carrousel et la barre d'information ne sont pas encore
thémables, les `extra` restent dessinés derrière eux (PR 11g).

## 11. Éléments et propriétés

Sources : `themes/ThemeSupport.cpp` @10.1.1. La table élément → propriétés est
`ElementMap` (`:175-617`), les noms XML des éléments sont dans `ElementType` (`:619-650`), ceux
des propriétés dans `PropertyName` (`:652-788`) et le type de chaque propriété dans
`InitializeStatics` (`:10-153`). Chaque ligne cite ces trois endroits.

Types de propriété chez Recalbox : `NormalizedPair`, `Path`, `String`, `Color`, `Float`,
`Boolean` (`themes/PropertyTypes.h:7-15`). Il n'y a **pas** de liste de chemins. Une propriété
`Path` dont le fichier n'existe pas est ignorée (`themes/ThemeData.cpp:467-475`) : le
composant garde sa valeur. Ce projet fait pareil, avec un avertissement `fichier:ligne`
(section 13).

**Valeurs par défaut.** Le moteur de thème de Recalbox n'en a pas : une propriété absente
n'est pas ajoutée à l'élément, et le composant garde sa propre valeur. Ce projet suit la même
règle (R5 : propriété absente = comportement actuel inchangé). Une PR qui donne un effet à
une propriété écrit la valeur du composant dans sa description.

**Statut ici.**
- `format 3` : déjà lue et appliquée (format d'Aloshi).
- `pris en charge (11d)` : propriétés communes (`zIndex`, `rotation`, `rotationOrigin`,
  `disabled`), appliquées par `GuiComponent` (`applyTheme`, `applyCommonTheme`) à tout
  composant qui lit l'élément ; détails et valeurs par défaut en section 10 (« Propriétés
  communes »).
- `11f` : éléments des menus thémés et clavier.
- `11g` : carrousel système.
- `11h` : le reste.

Toutes ces propriétés sont **acceptées** par le schéma dès maintenant (pas d'avertissement),
mais n'ont d'effet que dans la PR indiquée. Une PR ne traite que les lignes à son nom ; le
propriétaire peut reclasser une ligne en « non prise en charge ».

**Non lisibles chez Recalbox, donc absents ici** :
- `LogoList`, `CarouselList` et `GridList` sont dans `ElementMap` mais n'ont pas de nom XML
  dans `ElementType` ;
- `VerticalMargin` (élément `textlist`, `ThemeSupport.cpp:349`) n'a pas de nom XML dans
  `PropertyName`.

Le test « Schema: matches docs/THEMES.md section 11 » (`tests/unit/test_ThemeSchema.cpp`) lit
ces tableaux et échoue si le schéma (`es-core/src/theme/ThemeSchema.cpp`) en diffère.

#### `image`

Élément : `ThemeSupport.cpp:623`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:182`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:183`, nom `:769`, type `:133` | format 3 |
| `maxSize` | NormalizedPair | `ThemeSupport.cpp:184`, nom `:748`, type `:112` | format 3 |
| `keepratio` | Boolean | `ThemeSupport.cpp:185`, nom `:737`, type `:101` | 11h |
| `origin` | NormalizedPair | `ThemeSupport.cpp:186`, nom `:750`, type `:114` | format 3 |
| `rotation` | Float | `ThemeSupport.cpp:187`, nom `:757`, type `:121` | pris en charge (11d) |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:188`, nom `:758`, type `:122` | pris en charge (11d) |
| `path` | Path | `ThemeSupport.cpp:189`, nom `:751`, type `:115` | format 3 |
| `tile` | Boolean | `ThemeSupport.cpp:190`, nom `:773`, type `:137` | format 3 |
| `color` | Color | `ThemeSupport.cpp:191`, nom `:659`, type `:23` | format 3 |
| `colorTop` | Color | `ThemeSupport.cpp:192`, nom `:660`, type `:24` | 11h |
| `colorBottom` | Color | `ThemeSupport.cpp:193`, nom `:661`, type `:25` | 11h |
| `colorLeft` | Color | `ThemeSupport.cpp:194`, nom `:662`, type `:26` | 11h |
| `colorRight` | Color | `ThemeSupport.cpp:195`, nom `:663`, type `:27` | 11h |
| `colorTopLeft` | Color | `ThemeSupport.cpp:196`, nom `:664`, type `:28` | 11h |
| `colorTopRight` | Color | `ThemeSupport.cpp:197`, nom `:665`, type `:29` | 11h |
| `colorBottomLeft` | Color | `ThemeSupport.cpp:198`, nom `:666`, type `:30` | 11h |
| `colorBottomRight` | Color | `ThemeSupport.cpp:199`, nom `:667`, type `:31` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:200`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:201`, nom `:670`, type `:34` | pris en charge (11d) |
| `reflection` | NormalizedPair | `ThemeSupport.cpp:202`, nom `:756`, type `:120` | 11h |

#### `box`

Élément : `ThemeSupport.cpp:624`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:208`, nom `:754`, type `:118` | 11h |
| `size` | NormalizedPair | `ThemeSupport.cpp:209`, nom `:769`, type `:133` | 11h |
| `origin` | NormalizedPair | `ThemeSupport.cpp:210`, nom `:750`, type `:114` | 11h |
| `rotation` | Float | `ThemeSupport.cpp:211`, nom `:757`, type `:121` | 11h |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:212`, nom `:758`, type `:122` | 11h |
| `color` | Color | `ThemeSupport.cpp:213`, nom `:659`, type `:23` | 11h |
| `colorTop` | Color | `ThemeSupport.cpp:214`, nom `:660`, type `:24` | 11h |
| `colorBottom` | Color | `ThemeSupport.cpp:215`, nom `:661`, type `:25` | 11h |
| `colorLeft` | Color | `ThemeSupport.cpp:216`, nom `:662`, type `:26` | 11h |
| `colorRight` | Color | `ThemeSupport.cpp:217`, nom `:663`, type `:27` | 11h |
| `colorTopLeft` | Color | `ThemeSupport.cpp:218`, nom `:664`, type `:28` | 11h |
| `colorTopRight` | Color | `ThemeSupport.cpp:219`, nom `:665`, type `:29` | 11h |
| `colorBottomLeft` | Color | `ThemeSupport.cpp:220`, nom `:666`, type `:30` | 11h |
| `colorBottomRight` | Color | `ThemeSupport.cpp:221`, nom `:667`, type `:31` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:222`, nom `:776`, type `:141` | 11h |
| `disabled` | Boolean | `ThemeSupport.cpp:223`, nom `:670`, type `:34` | 11h |

#### `video`

Élément : `ThemeSupport.cpp:625`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:229`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:230`, nom `:769`, type `:133` | format 3 |
| `maxSize` | NormalizedPair | `ThemeSupport.cpp:231`, nom `:748`, type `:112` | format 3 |
| `keepratio` | Boolean | `ThemeSupport.cpp:232`, nom `:737`, type `:101` | 11h |
| `origin` | NormalizedPair | `ThemeSupport.cpp:233`, nom `:750`, type `:114` | format 3 |
| `rotation` | Float | `ThemeSupport.cpp:234`, nom `:757`, type `:121` | pris en charge (11d) |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:235`, nom `:758`, type `:122` | pris en charge (11d) |
| `path` | Path | `ThemeSupport.cpp:236`, nom `:751`, type `:115` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:237`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:238`, nom `:670`, type `:34` | pris en charge (11d) |
| `animations` | String | `ThemeSupport.cpp:239`, nom `:657`, type `:21` | 11h |
| `loops` | Float | `ThemeSupport.cpp:240`, nom `:746`, type `:110` | 11h |
| `delay` | Float | `ThemeSupport.cpp:241`, nom `:669`, type `:33` | format 3 |
| `reflection` | NormalizedPair | `ThemeSupport.cpp:242`, nom `:756`, type `:120` | 11h |
| `link` | String | `ThemeSupport.cpp:243`, nom `:739`, type `:103` | 11h |
| `default` | Path | format 3 d'Aloshi (absente de Recalbox) | format 3 |
| `showSnapshotNoVideo` | Boolean | format 3 d'Aloshi (absente de Recalbox) | format 3 |
| `showSnapshotDelay` | Boolean | format 3 d'Aloshi (absente de Recalbox) | format 3 |

#### `text`

Élément : `ThemeSupport.cpp:626`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:249`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:250`, nom `:769`, type `:133` | format 3 |
| `origin` | NormalizedPair | `ThemeSupport.cpp:251`, nom `:750`, type `:114` | 11h |
| `rotation` | Float | `ThemeSupport.cpp:252`, nom `:757`, type `:121` | pris en charge (11d) |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:253`, nom `:758`, type `:122` | pris en charge (11d) |
| `text` | String | `ThemeSupport.cpp:254`, nom `:770`, type `:134` | format 3 |
| `path` | Path | `ThemeSupport.cpp:255`, nom `:751`, type `:115` | 11h |
| `backgroundColor` | Color | `ThemeSupport.cpp:256`, nom `:658`, type `:22` | 11h |
| `fontPath` | Path | `ThemeSupport.cpp:257`, nom `:674`, type `:38` | format 3 |
| `fontSize` | Float | `ThemeSupport.cpp:258`, nom `:675`, type `:39` | format 3 |
| `fontStyle` | String | `ThemeSupport.cpp:259`, nom `:676`, type `:40` | 11h |
| `color` | Color | `ThemeSupport.cpp:260`, nom `:659`, type `:23` | format 3 |
| `alignment` | String | `ThemeSupport.cpp:261`, nom `:656`, type `:20` | format 3 |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:262`, nom `:678`, type `:42` | format 3 |
| `lineSpacing` | Float | `ThemeSupport.cpp:263`, nom `:738`, type `:102` | format 3 |
| `multiline` | Boolean | `ThemeSupport.cpp:264`, nom `:749`, type `:113` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:265`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:266`, nom `:670`, type `:34` | pris en charge (11d) |

#### `scrolltext`

Élément : `ThemeSupport.cpp:627`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:292`, nom `:754`, type `:118` | 11h |
| `size` | NormalizedPair | `ThemeSupport.cpp:293`, nom `:769`, type `:133` | 11h |
| `origin` | NormalizedPair | `ThemeSupport.cpp:294`, nom `:750`, type `:114` | 11h |
| `rotation` | Float | `ThemeSupport.cpp:295`, nom `:757`, type `:121` | 11h |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:296`, nom `:758`, type `:122` | 11h |
| `text` | String | `ThemeSupport.cpp:297`, nom `:770`, type `:134` | 11h |
| `path` | Path | `ThemeSupport.cpp:298`, nom `:751`, type `:115` | 11h |
| `backgroundColor` | Color | `ThemeSupport.cpp:299`, nom `:658`, type `:22` | 11h |
| `fontPath` | Path | `ThemeSupport.cpp:300`, nom `:674`, type `:38` | 11h |
| `fontSize` | Float | `ThemeSupport.cpp:301`, nom `:675`, type `:39` | 11h |
| `fontStyle` | String | `ThemeSupport.cpp:302`, nom `:676`, type `:40` | 11h |
| `color` | Color | `ThemeSupport.cpp:303`, nom `:659`, type `:23` | 11h |
| `alignment` | String | `ThemeSupport.cpp:304`, nom `:656`, type `:20` | 11h |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:305`, nom `:678`, type `:42` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:306`, nom `:776`, type `:141` | 11h |
| `disabled` | Boolean | `ThemeSupport.cpp:307`, nom `:670`, type `:34` | 11h |

#### `markdown`

Élément : `ThemeSupport.cpp:628`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:272`, nom `:754`, type `:118` | 11h |
| `size` | NormalizedPair | `ThemeSupport.cpp:273`, nom `:769`, type `:133` | 11h |
| `origin` | NormalizedPair | `ThemeSupport.cpp:274`, nom `:750`, type `:114` | 11h |
| `rotation` | Float | `ThemeSupport.cpp:275`, nom `:757`, type `:121` | 11h |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:276`, nom `:758`, type `:122` | 11h |
| `text` | String | `ThemeSupport.cpp:277`, nom `:770`, type `:134` | 11h |
| `path` | Path | `ThemeSupport.cpp:278`, nom `:751`, type `:115` | 11h |
| `fontPath` | Path | `ThemeSupport.cpp:279`, nom `:674`, type `:38` | 11h |
| `fontSize` | Float | `ThemeSupport.cpp:280`, nom `:675`, type `:39` | 11h |
| `color` | Color | `ThemeSupport.cpp:281`, nom `:659`, type `:23` | 11h |
| `alignment` | String | `ThemeSupport.cpp:282`, nom `:656`, type `:20` | 11h |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:283`, nom `:678`, type `:42` | 11h |
| `lineSpacing` | Float | `ThemeSupport.cpp:284`, nom `:738`, type `:102` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:285`, nom `:776`, type `:141` | 11h |
| `disabled` | Boolean | `ThemeSupport.cpp:286`, nom `:670`, type `:34` | 11h |

#### `textlist`

Élément : `ThemeSupport.cpp:629`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:313`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:314`, nom `:769`, type `:133` | format 3 |
| `origin` | NormalizedPair | `ThemeSupport.cpp:315`, nom `:750`, type `:114` | 11h |
| `selectorHeight` | Float | `ThemeSupport.cpp:316`, nom `:763`, type `:127` | 11h |
| `selectorOffsetY` | Float | `ThemeSupport.cpp:317`, nom `:767`, type `:131` | 11h |
| `selectorColor` | Color | `ThemeSupport.cpp:318`, nom `:762`, type `:126` | format 3 |
| `selectorImagePath` | Path | `ThemeSupport.cpp:319`, nom `:764`, type `:128` | 11h |
| `selectorImageTile` | Boolean | `ThemeSupport.cpp:320`, nom `:765`, type `:129` | 11h |
| `selectedColor` | Color | `ThemeSupport.cpp:321`, nom `:761`, type `:125` | format 3 |
| `primaryColor` | Color | `ThemeSupport.cpp:322`, nom `:755`, type `:119` | format 3 |
| `secondaryColor` | Color | `ThemeSupport.cpp:323`, nom `:760`, type `:124` | format 3 |
| `fontPath` | Path | `ThemeSupport.cpp:324`, nom `:674`, type `:38` | format 3 |
| `fontSize` | Float | `ThemeSupport.cpp:325`, nom `:675`, type `:39` | format 3 |
| `scrollSound` | Path | `ThemeSupport.cpp:326`, nom `:759`, type `:123` | format 3 |
| `alignment` | String | `ThemeSupport.cpp:327`, nom `:656`, type `:20` | format 3 |
| `horizontalMargin` | Float | `ThemeSupport.cpp:328`, nom `:680`, type `:44` | format 3 |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:329`, nom `:678`, type `:42` | format 3 |
| `lineSpacing` | Float | `ThemeSupport.cpp:330`, nom `:738`, type `:102` | format 3 |
| `zIndex` | Float | `ThemeSupport.cpp:331`, nom `:776`, type `:141` | pris en charge (11d) |

#### `container`

Élément : `ThemeSupport.cpp:630`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:376`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:377`, nom `:769`, type `:133` | format 3 |
| `origin` | NormalizedPair | `ThemeSupport.cpp:378`, nom `:750`, type `:114` | 11h |
| `zIndex` | Float | `ThemeSupport.cpp:379`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:380`, nom `:670`, type `:34` | pris en charge (11d) |

#### `ninepatch`

Élément : `ThemeSupport.cpp:631`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:386`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:387`, nom `:769`, type `:133` | format 3 |
| `path` | Path | `ThemeSupport.cpp:388`, nom `:751`, type `:115` | format 3 |
| `zIndex` | Float | `ThemeSupport.cpp:389`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:390`, nom `:670`, type `:34` | pris en charge (11d) |

#### `datetime`

Élément : `ThemeSupport.cpp:632`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:396`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:397`, nom `:769`, type `:133` | format 3 |
| `origin` | NormalizedPair | `ThemeSupport.cpp:398`, nom `:750`, type `:114` | 11h |
| `color` | Color | `ThemeSupport.cpp:399`, nom `:659`, type `:23` | format 3 |
| `backgroundColor` | Color | `ThemeSupport.cpp:400`, nom `:658`, type `:22` | 11h |
| `fontPath` | Path | `ThemeSupport.cpp:401`, nom `:674`, type `:38` | format 3 |
| `fontSize` | Float | `ThemeSupport.cpp:402`, nom `:675`, type `:39` | format 3 |
| `alignment` | String | `ThemeSupport.cpp:403`, nom `:656`, type `:20` | 11h |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:404`, nom `:678`, type `:42` | format 3 |
| `zIndex` | Float | `ThemeSupport.cpp:405`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:406`, nom `:670`, type `:34` | pris en charge (11d) |
| `display` | String | `ThemeSupport.cpp:407`, nom `:671`, type `:35` | 11h |

#### `rating`

Élément : `ThemeSupport.cpp:633`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:413`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:414`, nom `:769`, type `:133` | format 3 |
| `origin` | NormalizedPair | `ThemeSupport.cpp:415`, nom `:750`, type `:114` | 11h |
| `rotation` | Float | `ThemeSupport.cpp:416`, nom `:757`, type `:121` | pris en charge (11d) |
| `rotationOrigin` | NormalizedPair | `ThemeSupport.cpp:417`, nom `:758`, type `:122` | pris en charge (11d) |
| `filledPath` | Path | `ThemeSupport.cpp:418`, nom `:673`, type `:37` | format 3 |
| `unfilledPath` | Path | `ThemeSupport.cpp:419`, nom `:775`, type `:139` | format 3 |
| `zIndex` | Float | `ThemeSupport.cpp:420`, nom `:776`, type `:141` | pris en charge (11d) |
| `disabled` | Boolean | `ThemeSupport.cpp:421`, nom `:670`, type `:34` | pris en charge (11d) |

#### `sound`

Élément : `ThemeSupport.cpp:634`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `path` | Path | `ThemeSupport.cpp:427`, nom `:751`, type `:115` | format 3 |

#### `helpsystem`

Élément : `ThemeSupport.cpp:635`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pos` | NormalizedPair | `ThemeSupport.cpp:433`, nom `:754`, type `:118` | format 3 |
| `size` | NormalizedPair | `ThemeSupport.cpp:434`, nom `:769`, type `:133` | 11h |
| `textColor` | Color | `ThemeSupport.cpp:435`, nom `:771`, type `:135` | format 3 |
| `iconColor` | Color | `ThemeSupport.cpp:436`, nom `:687`, type `:49` | format 3 |
| `fontPath` | Path | `ThemeSupport.cpp:437`, nom `:674`, type `:38` | format 3 |
| `fontSize` | Float | `ThemeSupport.cpp:438`, nom `:675`, type `:39` | format 3 |
| `iconUpDown` | Path | `ThemeSupport.cpp:439`, nom `:732`, type `:96` | 11h |
| `iconLeftRight` | Path | `ThemeSupport.cpp:440`, nom `:712`, type `:76` | 11h |
| `iconUpDownLeftRight` | Path | `ThemeSupport.cpp:441`, nom `:733`, type `:97` | 11h |
| `iconA` | Path | `ThemeSupport.cpp:442`, nom `:681`, type `:45` | 11h |
| `iconB` | Path | `ThemeSupport.cpp:443`, nom `:684`, type `:48` | 11h |
| `iconX` | Path | `ThemeSupport.cpp:444`, nom `:735`, type `:99` | 11h |
| `iconY` | Path | `ThemeSupport.cpp:445`, nom `:736`, type `:100` | 11h |
| `iconL` | Path | `ThemeSupport.cpp:446`, nom `:707`, type `:71` | 11h |
| `iconL2` | Path | `ThemeSupport.cpp:447`, nom `:708`, type `:72` | 11h |
| `iconL2R2` | Path | `ThemeSupport.cpp:448`, nom `:709`, type `:73` | 11h |
| `iconL3` | Path | `ThemeSupport.cpp:449`, nom `:710`, type `:74` | 11h |
| `iconL3R3` | Path | `ThemeSupport.cpp:450`, nom `:711`, type `:75` | 11h |
| `iconLR` | Path | `ThemeSupport.cpp:451`, nom `:715`, type `:79` | 11h |
| `iconR` | Path | `ThemeSupport.cpp:452`, nom `:718`, type `:82` | 11h |
| `iconR2` | Path | `ThemeSupport.cpp:453`, nom `:719`, type `:83` | 11h |
| `iconR3` | Path | `ThemeSupport.cpp:454`, nom `:720`, type `:84` | 11h |
| `iconStart` | Path | `ThemeSupport.cpp:455`, nom `:727`, type `:91` | 11h |
| `iconSelect` | Path | `ThemeSupport.cpp:456`, nom `:724`, type `:88` | 11h |
| `iconHotkey` | Path | `ThemeSupport.cpp:457`, nom `:699`, type `:63` | 11h |
| `iconHkA` | Path | `ThemeSupport.cpp:458`, nom `:692`, type `:56` | 11h |
| `iconHkB` | Path | `ThemeSupport.cpp:459`, nom `:693`, type `:57` | 11h |
| `iconHkL` | Path | `ThemeSupport.cpp:460`, nom `:694`, type `:58` | 11h |
| `iconHkR` | Path | `ThemeSupport.cpp:461`, nom `:695`, type `:59` | 11h |
| `iconHkX` | Path | `ThemeSupport.cpp:462`, nom `:696`, type `:60` | 11h |
| `iconHkY` | Path | `ThemeSupport.cpp:463`, nom `:697`, type `:61` | 11h |
| `iconHkLeftRight` | Path | `ThemeSupport.cpp:464`, nom `:698`, type `:62` | 11h |
| `iconJ1UpDownLeftRight` | Path | `ThemeSupport.cpp:465`, nom `:702`, type `:66` | 11h |
| `iconJ1UpDown` | Path | `ThemeSupport.cpp:466`, nom `:701`, type `:65` | 11h |
| `iconJ1LeftRight` | Path | `ThemeSupport.cpp:467`, nom `:700`, type `:64` | 11h |
| `iconJ2UpDownLeftRight` | Path | `ThemeSupport.cpp:468`, nom `:705`, type `:69` | 11h |
| `iconJ2UpDown` | Path | `ThemeSupport.cpp:469`, nom `:704`, type `:68` | 11h |
| `iconJ2LeftRight` | Path | `ThemeSupport.cpp:470`, nom `:703`, type `:67` | 11h |

#### `carousel`

Élément : `ThemeSupport.cpp:636`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `type` | String | `ThemeSupport.cpp:476`, nom `:774`, type `:138` | 11g |
| `size` | NormalizedPair | `ThemeSupport.cpp:477`, nom `:769`, type `:133` | 11g |
| `pos` | NormalizedPair | `ThemeSupport.cpp:478`, nom `:754`, type `:118` | 11g |
| `origin` | NormalizedPair | `ThemeSupport.cpp:479`, nom `:750`, type `:114` | 11g |
| `color` | Color | `ThemeSupport.cpp:480`, nom `:659`, type `:23` | 11g |
| `fontPath` | Path | `ThemeSupport.cpp:481`, nom `:674`, type `:38` | 11g |
| `fontSize` | Float | `ThemeSupport.cpp:482`, nom `:675`, type `:39` | 11g |
| `fontColor` | Color | `ThemeSupport.cpp:483`, nom `:677`, type `:41` | 11g |
| `logoScale` | Float | `ThemeSupport.cpp:484`, nom `:743`, type `:107` | 11g |
| `logoRotation` | Float | `ThemeSupport.cpp:485`, nom `:741`, type `:105` | 11g |
| `logoRotationOrigin` | NormalizedPair | `ThemeSupport.cpp:486`, nom `:742`, type `:106` | 11g |
| `logoSize` | NormalizedPair | `ThemeSupport.cpp:487`, nom `:744`, type `:108` | 11g |
| `textOnly` | Boolean | `ThemeSupport.cpp:488`, nom `:745`, type `:109` | 11g |
| `forceUppercase` | Boolean | `ThemeSupport.cpp:489`, nom `:678`, type `:42` | 11g |
| `logoAlignment` | String | `ThemeSupport.cpp:490`, nom `:740`, type `:104` | 11g |
| `maxLogoCount` | Float | `ThemeSupport.cpp:491`, nom `:747`, type `:111` | 11g |
| `selectorColor` | Color | `ThemeSupport.cpp:492`, nom `:762`, type `:126` | 11g |
| `selectedColor` | Color | `ThemeSupport.cpp:493`, nom `:761`, type `:125` | 11g |
| `primaryColor` | Color | `ThemeSupport.cpp:494`, nom `:755`, type `:119` | 11g |
| `secondaryColor` | Color | `ThemeSupport.cpp:495`, nom `:760`, type `:124` | 11g |
| `selectorHeight` | Float | `ThemeSupport.cpp:496`, nom `:763`, type `:127` | 11g |
| `selectorOffsetX` | Float | `ThemeSupport.cpp:497`, nom `:766`, type `:130` | 11g |
| `selectorOffsetY` | Float | `ThemeSupport.cpp:498`, nom `:767`, type `:131` | 11g |
| `textOffsetX` | Float | `ThemeSupport.cpp:499`, nom `:772`, type `:136` | 11g |
| `lineSpacing` | Float | `ThemeSupport.cpp:500`, nom `:738`, type `:102` | 11g |
| `horizontalMargin` | Float | `ThemeSupport.cpp:501`, nom `:680`, type `:44` | 11g |
| `defaultTransition` | String | `ThemeSupport.cpp:502`, nom `:668`, type `:32` | 11g |
| `zIndex` | Float | `ThemeSupport.cpp:503`, nom `:776`, type `:141` | 11g |

#### `menuBackground`

Élément : `ThemeSupport.cpp:637`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `color` | Color | `ThemeSupport.cpp:509`, nom `:659`, type `:23` | 11f |
| `path` | Path | `ThemeSupport.cpp:510`, nom `:751`, type `:115` | 11f |
| `fadePath` | Path | `ThemeSupport.cpp:511`, nom `:672`, type `:36` | 11f |

#### `menuIcons`

Élément : `ThemeSupport.cpp:638`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `iconKodi` | Path | `ThemeSupport.cpp:517`, nom `:706`, type `:70` | 11f |
| `iconSystem` | Path | `ThemeSupport.cpp:518`, nom `:728`, type `:92` | 11f |
| `iconUpdates` | Path | `ThemeSupport.cpp:519`, nom `:734`, type `:98` | 11f |
| `iconControllers` | Path | `ThemeSupport.cpp:520`, nom `:688`, type `:52` | 11f |
| `iconGames` | Path | `ThemeSupport.cpp:521`, nom `:691`, type `:55` | 11f |
| `iconUI` | Path | `ThemeSupport.cpp:522`, nom `:731`, type `:95` | 11f |
| `iconSound` | Path | `ThemeSupport.cpp:523`, nom `:726`, type `:90` | 11f |
| `iconNetwork` | Path | `ThemeSupport.cpp:524`, nom `:716`, type `:80` | 11f |
| `iconScraper` | Path | `ThemeSupport.cpp:525`, nom `:723`, type `:87` | 11f |
| `iconAdvanced` | Path | `ThemeSupport.cpp:526`, nom `:682`, type `:46` | 11f |
| `iconQuit` | Path | `ThemeSupport.cpp:527`, nom `:717`, type `:81` | 11f |
| `iconRestart` | Path | `ThemeSupport.cpp:528`, nom `:722`, type `:86` | 11f |
| `iconShutdown` | Path | `ThemeSupport.cpp:529`, nom `:725`, type `:89` | 11f |
| `iconFastShutdown` | Path | `ThemeSupport.cpp:530`, nom `:690`, type `:54` | 11f |
| `iconLicense` | Path | `ThemeSupport.cpp:531`, nom `:713`, type `:77` | 11f |
| `iconList` | Path | `ThemeSupport.cpp:532`, nom `:714`, type `:78` | 11f |
| `iconBios` | Path | `ThemeSupport.cpp:533`, nom `:685`, type `:50` | 11f |
| `iconRecalboxRGBDual` | Path | `ThemeSupport.cpp:534`, nom `:721`, type `:85` | 11f |
| `iconTate` | Path | `ThemeSupport.cpp:535`, nom `:729`, type `:93` | 11f |
| `iconArcade` | Path | `ThemeSupport.cpp:536`, nom `:683`, type `:47` | 11f |
| `iconDownload` | Path | `ThemeSupport.cpp:537`, nom `:689`, type `:53` | 11f |
| `iconThemes` | Path | `ThemeSupport.cpp:538`, nom `:730`, type `:94` | 11f |
| `iconCardReader` | Path | `ThemeSupport.cpp:539`, nom `:686`, type `:51` | 11f |

#### `menuSwitch`

Élément : `ThemeSupport.cpp:639`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `pathOn` | Path | `ThemeSupport.cpp:545`, nom `:753`, type `:117` | 11f |
| `pathOff` | Path | `ThemeSupport.cpp:546`, nom `:752`, type `:116` | 11f |

#### `menuSlider`

Élément : `ThemeSupport.cpp:640`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `path` | Path | `ThemeSupport.cpp:552`, nom `:751`, type `:115` | 11f |

#### `menuButton`

Élément : `ThemeSupport.cpp:641`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `path` | Path | `ThemeSupport.cpp:558`, nom `:751`, type `:115` | 11f |
| `filledPath` | Path | `ThemeSupport.cpp:559`, nom `:673`, type `:37` | 11f |

#### `menuText`

Élément : `ThemeSupport.cpp:642`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `fontPath` | Path | `ThemeSupport.cpp:565`, nom `:674`, type `:38` | 11f |
| `fontSize` | Float | `ThemeSupport.cpp:566`, nom `:675`, type `:39` | 11f |
| `color` | Color | `ThemeSupport.cpp:567`, nom `:659`, type `:23` | 11f |
| `separatorColor` | Color | `ThemeSupport.cpp:568`, nom `:768`, type `:132` | 11f |
| `selectedColor` | Color | `ThemeSupport.cpp:569`, nom `:761`, type `:125` | 11f |
| `selectorColor` | Color | `ThemeSupport.cpp:570`, nom `:762`, type `:126` | 11f |

#### `menuTextSmall`

Élément : `ThemeSupport.cpp:643`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `fontPath` | Path | `ThemeSupport.cpp:576`, nom `:674`, type `:38` | 11f |
| `fontSize` | Float | `ThemeSupport.cpp:577`, nom `:675`, type `:39` | 11f |
| `color` | Color | `ThemeSupport.cpp:578`, nom `:659`, type `:23` | 11f |
| `selectedColor` | Color | `ThemeSupport.cpp:579`, nom `:761`, type `:125` | 11f |
| `selectorColor` | Color | `ThemeSupport.cpp:580`, nom `:762`, type `:126` | 11f |

#### `menuSection`

Élément : `ThemeSupport.cpp:644`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `fontPath` | Path | `ThemeSupport.cpp:586`, nom `:674`, type `:38` | 11f |
| `fontSize` | Float | `ThemeSupport.cpp:587`, nom `:675`, type `:39` | 11f |
| `color` | Color | `ThemeSupport.cpp:588`, nom `:659`, type `:23` | 11f |
| `selectedColor` | Color | `ThemeSupport.cpp:589`, nom `:761`, type `:125` | 11f |
| `alignment` | String | `ThemeSupport.cpp:590`, nom `:656`, type `:20` | 11f |

#### `menuSize`

Élément : `ThemeSupport.cpp:645`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `height` | Float | `ThemeSupport.cpp:596`, nom `:679`, type `:43` | 11f |

#### `keyboard`

Élément : `ThemeSupport.cpp:646`.

| Propriété | Type | Source | Statut ici |
|---|---|---|---|
| `keyColor` | Color | `ThemeSupport.cpp:604`, nom `:777`, type `:142` | 11f |
| `keySelectedColor` | Color | `ThemeSupport.cpp:605`, nom `:778`, type `:143` | 11f |
| `keyTextColor` | Color | `ThemeSupport.cpp:606`, nom `:779`, type `:144` | 11f |
| `keyDisabledColor` | Color | `ThemeSupport.cpp:607`, nom `:780`, type `:145` | 11f |
| `keyModifierColor` | Color | `ThemeSupport.cpp:608`, nom `:781`, type `:146` | 11f |
| `keyModifierLockedColor` | Color | `ThemeSupport.cpp:609`, nom `:782`, type `:147` | 11f |
| `keyTitleColor` | Color | `ThemeSupport.cpp:610`, nom `:783`, type `:148` | 11f |
| `keyEditTextColor` | Color | `ThemeSupport.cpp:611`, nom `:784`, type `:149` | 11f |
| `fontPath` | Path | `ThemeSupport.cpp:612`, nom `:674`, type `:38` | 11f |

## 12. Attributs d'élément

- `name` : nom de l'élément ; plusieurs noms séparés par des virgules définissent plusieurs
  éléments identiques (format 3).
- `extra="true"` : élément décoratif ajouté à la vue par `ThemeData::makeExtras`, dessiné dans
  l'ordre de déclaration (format 3). Ces éléments sont des enfants de `ThemeExtras`
  (`es-core/src/ThemeData.cpp`, `setExtras`) : sans cela, rien n'est dessiné (régression de
  #75, corrigée par #76).
- `if`, `ifexists`, `ifnotexists` : section 7. `region` : section 9.
- `showIf` : visibilité limitée à un type d'entrée (`game`, `folder`…), `ThemeData.cpp:335` ;
  ici, ignoré, ce n'est pas une propriété (11h).
- Tout autre attribut est une propriété (format 4) : `<text name="t" pos="0.1 0.2"/>` vaut
  `<text name="t"><pos>0.1 0.2</pos></text>`, suffixe localisé compris (`text.fr="…"`)
  (`themes/ThemeData.cpp:377-393` ; attributs exclus `:64-71`).
- Une propriété en nœud enfant l'emporte sur l'attribut du même nom ; elle a ses propres
  conditions (`if`, `ifexists`, `ifnotexists`, `region`) et peut donner sa valeur dans un
  attribut `value` : `<pos if="hd" value="0.1 0.2"/>` (`ThemeData.cpp:395-412`).

## 13. Diagnostics

- Format : `fichier:ligne: message`, journalisés une fois par chargement
  (`es::theme::Diagnostics::log`), écrits sur la sortie standard par `--check-theme`.
- Avertissement, nœud ignoré, le reste du thème chargé : élément inconnu, propriété inconnue,
  valeur invalide, fichier introuvable, include manquant, vide ou inutilisable (section 4),
  cycle, identifiant de condition inconnu, ressource intégrée `:/` (section 14).
- Erreur : fichier principal illisible, XML mal formé, `<theme>` ou `<formatVersion>`
  absent. Le système est alors affiché sans thème ; aucune exception ne sort de
  `SystemData::loadTheme` ni de `ThemeData::getDefault` (R6).

## 14. Écarts avec Recalbox

- Pas de matériel Recalbox (CRT, JAMMA, TATE, cartes) : identifiants à `false` (section 7).
- Chemins via XDG (`es::paths`), pas de chemins en dur.
- Pas d'OpenGL ES : moteur OpenGL 3.3.
- Détection des cycles d'inclusion (section 4).
- Le format 3 d'Aloshi reste lu à l'identique (R5).
- Ressources intégrées : Recalbox embarque des fichiers dans son exécutable, nommés `:/…`
  (ses valeurs par défaut en utilisent, `MenuThemeData.cpp:121-143`), et un thème peut y faire
  référence (par exemple une police `:/…ttf`). Ce projet n'a pas ces fichiers : une valeur
  `:/…` est un fichier introuvable, avertissement et propriété ignorée (la valeur par défaut
  du composant reste).

## 15. Non vérifié ou à trancher

- Vue `gamelistlogo` (section 10).
- Classement des propriétés `11h` de la section 11 : le propriétaire peut en passer en « non
  prise en charge ».
- Suffixes localisés de 5 lettres (`text.fr_FR`) : le chargeur compare aujourd'hui le suffixe à
  la région, à la langue (`fr`) et au pays (`FR`), pas à `fr_FR` (section 9).
- Élément absent du thème : Recalbox masque le composant (`Themable.cpp:19-21`) ; ce projet le
  laisse affiché avec ses valeurs, comme le format 3 (R5).
