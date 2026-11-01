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
> **État.** Les sections 1 à 10, 12 et 13 sont vérifiées. La section 11 (éléments et
> propriétés) est **INCOMPLÈTE** : ses tableaux sont à écrire dans la PR 11c2
> (`docs/phase11/11c2.md`) avant tout autre code de la phase.

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
- **État dans ce projet** : `ThemeSet::getThemePath` n'essaie encore que le cas 1 ; les cas 2
  et 3 sont à faire en 11c2. `--check-theme` applique déjà les trois.

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
  (section 8).
- Fichier manquant : avertissement `fichier:ligne`, inclusion ignorée. Cycle ou profondeur
  excessive : avertissement, inclusion ignorée (décision du projet, Recalbox ne détecte pas les
  cycles).
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
- **Dans ce projet** : seule `root` est fournie aujourd'hui. La liste retenue (et la valeur de
  chacune sur PC) est à écrire en 11c2, avec un tableau « variable / source / valeur ici ».

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

**Dans ce projet** : `applyLegacySubstitutions` (`es-core/src/theme/ThemeVariables.h`)
existe, mais `ThemeData::loadFile` lui passe un système vide. Le contexte (système, langue,
pays) est à brancher en 11c2 ; les substitutions liées à des données que ce projet n'a pas
(`$netplay`, `$lightgun`…) sont listées « non prises en charge » en 11c2.

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
| `virtual`, `arcade`, `port` | nature du système | `false` (à revoir en 11c2 si le système est connu) |
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
- Menu « Options du thème » : PR 11e.

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
| `menu` | menus thémés : PR 11f |
| `gameclip` | économiseur d'écran : PR 11h |
| `gamelistlogo` | **contradiction** : testée par `ThemeData::HasLogoView` (`themes/ThemeData.h:93`) mais absente de `SupportedViews` ; à trancher en 11c2 (R13) |
| `challenge` | non prise en charge (pas de système de défis) : avertissement, vue ignorée |

## 11. Éléments et propriétés — INCOMPLET

> À écrire **en entier** dans la PR 11c2 avant tout autre code (R4). Sans ces tableaux, le
> schéma (`es-core/src/theme/ThemeSchema.cpp`) n'a pas de source vérifiable.

Format attendu, un tableau par type d'élément :

| Propriété | Type | Défaut | Source (`fichier:ligne` @10.1.1) | Statut ici (PR) |
|---|---|---|---|---|

Sources : `themes/ElementTypes.h` (types d'éléments), `themes/PropertyNames.h` et
`themes/ThemeSupport.cpp` (propriétés par élément et leur type, par exemple `zIndex` : `Float`,
`ThemeSupport.cpp:141`), `themes/ThemeElement.{h,cpp}` (valeurs par défaut),
`themes/MenuThemeData.{h,cpp}` (éléments de la vue `menu`).

Types de propriété : `NormalizedPair`, `Path`, `PathList`, `String`, `Color`, `Float`,
`Boolean` (`themes/PropertyTypes.h`).

## 12. Attributs d'élément

- `name` : nom de l'élément ; plusieurs noms séparés par des virgules définissent plusieurs
  éléments identiques (format 3).
- `extra="true"` : élément décoratif ajouté à la vue par `ThemeData::makeExtras`, dessiné dans
  l'ordre de déclaration (format 3). Ces éléments sont des enfants de `ThemeExtras`
  (`es-core/src/ThemeData.cpp`, `setExtras`) : sans cela, rien n'est dessiné (régression de
  #75, corrigée par #76).
- `if`, `ifexists`, `ifnotexists` : section 7. `region` : section 9.

## 13. Diagnostics

- Format : `fichier:ligne: message`, journalisés une fois par chargement
  (`es::theme::Diagnostics::log`), écrits sur la sortie standard par `--check-theme`.
- Avertissement, nœud ignoré, le reste du thème chargé : élément inconnu, propriété inconnue,
  valeur invalide, fichier introuvable, include manquant, cycle, identifiant de condition
  inconnu.
- Erreur : fichier principal illisible, XML mal formé, `<theme>` ou `<formatVersion>`
  absent. Le système est alors affiché sans thème ; aucune exception ne sort de
  `SystemData::loadTheme` ni de `ThemeData::getDefault` (R6).

## 14. Écarts avec Recalbox

- Pas de matériel Recalbox (CRT, JAMMA, TATE, cartes) : identifiants à `false` (section 7).
- Chemins via XDG (`es::paths`), pas de chemins en dur.
- Pas d'OpenGL ES : moteur OpenGL 3.3.
- Détection des cycles d'inclusion (section 4).
- Le format 3 d'Aloshi reste lu à l'identique (R5).

## 15. Non vérifié ou à trancher

- Vue `gamelistlogo` (section 10).
- Liste des variables et substitutions retenues, et valeur de chacune sur PC (sections 5 et 6).
- Éléments et propriétés (section 11).
