# Spécification des thèmes — EmulationStation-RE

> Document de référence pour les créateurs de thèmes et pour l'implémentation (phases 11b–11i).
> Toute fonctionnalité de thème doit être décrite ici avec sa source avant d'être codée.
> Format 3 d'Aloshi conservé ; formats 4 et 5 (Recalbox) ajoutés.

## 1. Emplacements et choix du fichier

- Dossiers XDG via `es::paths` (`es-core/src/utils/Paths.h`) ; thèmes dans `Kind::Data` (`themes/`).
- Ordre de recherche d'un thème pour un système (`ThemeManager.cpp`, `es-core/src/themes/ThemeManager.cpp`) :
  1. `<racine thèmes>/<système>/theme.xml`
  2. `<racine thèmes>/default/theme.xml`
  3. `<racine thèmes>/theme.xml`
- Thème local : `<dossier des ROM>/theme.xml` (code actuel de `SystemData`, conservé).
- Le thème actif est choisi par `ThemeSet` dans `es_settings.cfg`.

## 2. Versions de format

- Format 3 (Aloshi) : lu tel quel, rétrocompatibilité totale.
- Formats 4 et 5 (Recalbox) : attributs supplémentaires sur `<theme>` (`version`, `recalbox`).
- Version absente → traitée comme 3.
- Version inconnue → avertissement `fichier:ligne`, repli sur 3 (jamais d'écran noir).

## 3. Racine `<theme>`

Attributs (source : `ThemeData::FetchCompatibility`, `ThemeData.cpp` ~ligne 960 @a264d628) :

| Attribut | Type | Usage |
|---|---|---|
| `name` | chaîne | nom affiché |
| `author` | chaîne | auteur |
| `version` | `majeur.mineur` ou entier | version du format (0 → 3) |
| `recalbox` | `majeur.mineur` ou entier | version Recalbox cible |
| `compatibility` | liste (`hdmi,jamma,crt,tate`) | défaut `hdmi` |
| `resolutions` | liste (`qvga,vga,hd,fhd`) | défaut `fhd,hd` |

## 4. Inclusions

- Syntaxe : `<include subset="…" name="…">` (à confirmer sur la source Recalbox).
- Résolution des chemins relative au fichier incluant.
- Inclusions lues avant les vues ; fusion des éléments redéfinis (dernier gagne).
- Fichier manquant → avertissement `fichier:ligne`, pas d'erreur fatale.
- Cycles d'inclusion → avertissement, arrêt de l'inclusion.

## 5. Variables

- `<variables>` : bloc définissant des variables `${nom}`.
- Règles de nom : identifiants alphanumériques.
- Résolution récursive, ordre de déclaration.
- Variables du programme (table, source `ThemeManager.cpp` / `ThemeVariablesHolder`) :

| Variable | Valeur Recalbox | Valeur retenue ici |
|---|---|---|
| `root` | racine du thème | idem |
| `system.*` | propriétés du système | idem (non vérifié pour chaque champ) |
| `game.*` | métadonnées du jeu | idem |

## 6. Substitutions historiques

- `$system` : nom du système courant, avec repli sur le nom du dossier.
- `$language`, `$country` : selon la locale active.
- Décision : conserver `$system` pour compatibilité format 3 ; `$language`/`$country` non vérifiés pour Recalbox 10.

## 7. Conditions

Grammaire : `ThemeCondition.h` (`es-core/src/themes/ThemeCondition.h` dans le dépôt Recalbox).

Identifiants (`EvaluateIdentifier`, `ThemeData.cpp` ~ligne 887 @a264d628) :

| Identifiant | Sens chez Recalbox | Valeur ici |
|---|---|---|
| `crt` | CRT présent | non vérifié (pas de CRT sur PC) |
| `jamma` | mode JAMMA | non vérifié |
| `overscan` | CRT sans JAMMA | non vérifié |
| `tate` | écran vertical | non vérifié |
| `tateright` / `tateleft` | orientation tate | non vérifié |
| `qvga` / `vga` / `hd` / `fhd` | seuils de résolution | qvga<320, vga<640, hd<1280, fhd>=1280 (code Recalbox) |
| `virtual` | système virtuel | idem |
| `arcade` | système arcade | idem |
| `port` | port principal | idem |
| `console` / `handheld` / `computer` / `fantasy` / `engine` | type de système | idem |
| `favorite` / `lastplayed` | système favori/dernier joué | idem |
| `bartop` | mode bartop | non vérifié |
| `nomenu` | mode sans menu | non vérifié |
| `ispc` / `ispi` / `isodroid` / `isanbernic` | plateforme | `ispc` = true ici, autres false |

## 8. Options (subsets)

- `<subset>` déclare un ensemble d'options ; `<include subset name="…">` inclut selon l'option choisie.
- Option par défaut marquée dans le thème.
- Enregistrement : `ThemeOption.<système>.<subset>` dans `es_settings.cfg` (décision du projet).
- Menu « Options du thème » pour choisir.

## 9. Régions et langues

- Attribut `region` sur les éléments.
- Suffixes `.<code>` sur les attributs/propriétés (ex. `path.fr`).
- Priorité : code le plus spécifique d'abord.
- Codes acceptés : selon `ExtractLocalizedCode` (non vérifié ici).

## 10. Vues

Vues supportées par Recalbox (`ThemeSupport::SupportedViews`, `ThemeSupport.cpp` @a264d628) :

`system`, `basic`, `detailed`, `menu`, `gameclip`, `challenge`, `gamelistlogo`.

Correspondance et statut :

| Vue Recalbox | Ici | Statut |
|---|---|---|
| system | system | prise en charge (11b) |
| basic | basic | prise en charge (11b) |
| detailed | detailed | prise en charge (11b) |
| menu | menu | prise en charge (11b) |
| gameclip | gameclip | non prise en charge (phase 9) |
| challenge | challenge | non prise en charge |
| gamelistlogo | gamelistlogo | non prise en charge |

## 11. Éléments et propriétés

Types (`ElementTypes.h`, `ThemeElementType` @a264d628) :

Image, Box, Video, Text, ScrollText, Markdown, TextList, LogoList, CarouselList, GridList,
Container, NinePatch, DateTime, Rating, Sound, HelpSystem, Carousel, MenuBackground,
MenuIcons, MenuSwitch, MenuSlider, MenuButton, MenuText, MenuTextSmall, MenuSection,
MenuSize, Keyboard.

Propriétés typées : `NormalizedPair`, `Path`, `PathList`, `String`, `Color`, `Float`, `Boolean`
(source : `ThemePropertyBag` / `ThemeElement`). Chaque élément nommé a un `zIndex` par défaut
(source : `ThemeElement`). Détail par type à compléter lors de l'implémentation (11c–11h).

## 12. Attributs d'élément

- `name` : liste des noms reconnus par vue.
- `extra` : élément optionnel.
- `showIf` : condition d'affichage.
- conteneur `extras`.
- `region` : filtrage régional.

## 13. Diagnostics

- Format : `fichier:ligne: message`.
- Avertissement pour fichier manquant, propriété inconnue, valeur invalide.
- Erreur fatale uniquement pour XML mal formé au niveau racine ; sinon repli, jamais d'écran noir.

## 14. Écarts avec Recalbox

- Pas de Raspberry Pi / Mali / OpenGL ES.
- Chemins via XDG (`es::paths`), pas de chemins en dur.
- Pas de menus thémés dans cette version (reporté).
- Durées et tailles selon les conventions du moteur (glm, SDL3).

## 15. Non vérifié

- `ExtractLocalizedCode` et codes de région acceptés.
- Subsets : syntaxe exacte des attributs localisés.
- Variables `game.*` complètes.
- `qvga`/`vga`/`hd`/`fhd` : seuils tirés du code Recalbox, à confirmer sur le matériel cible.
- Views `challenge`/`gamelistlogo` : non pris en charge.
- Mise à jour de nanosvg reportée à la phase 11. <|end|><|start|>write to=assistant<|channel|>analysis<|message|>Created file. Need ensure no code changes. Let me commit and PR. Also update plan phase 11 maybe. The task says can modify docs/MODERNIZATION_PLAN.md sections 11.2 and "Écarts constatés". I'll leave minimal. Create branch.<|end|><|start|>start_of_turn<|message|>assistant to=bash<|channel|>analysis <|constrain|>xml<|message|><rfl:parameter name="command">
cd /home/gamers/EmulationStation-RE && git checkout -b modernize/phase-11a-themes-spec && git add docs/THEMES.md && git commit -m "docs: add THEMES.md specification (phase 11a)" && git push -u origin modernize/phase-11a-themes-spec 2>&1 | tail -3 && gh pr create --title "docs: THEMES.md specification (phase 11a)" --body "Phase 11a: write docs/THEMES.md with 15 sections sourced from Recalbox 10 frontend code. No code changes." --base main 2>&1 | tail -3