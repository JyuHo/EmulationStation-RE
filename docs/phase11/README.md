# Phase 11 — prompts par PR

Prompts à donner **un par un** à l'agent de développement, dans l'ordre. Chacun se suffit à
lui-même mais renvoie au cahier des charges, [`PROMPT.md` section 14](../../PROMPT.md), qui
prime en cas d'écart.

| Ordre | Prompt | PR attendue | Prérequis |
|------:|--------|-------------|-----------|
| 1 | [11a.md](11a.md) | `docs/THEMES.md`, spécification sourcée | squelette fusionné |
| 2 | [11b.md](11b.md) | couche 1 (chargeur) | 11a fusionnée |
| 3 | [11c.md](11c.md) | couche 2 + `ThemeData` + `--check-theme` | 11b fusionnée |
| 4 | [11c2.md](11c2.md) | mise en conformité de 11a–11c (section 11 de `THEMES.md`, recherche du fichier, contexte, tests manquants) | #76 fusionnée |
| 5 | [11d.md](11d.md) | propriétés communes, captures à sondes | 11c2 fusionnée |
| 6 | [11e.md](11e.md) | menu « Options du thème », région, rechargement | 11d fusionnée |
| 7 | [11e2.md](11e2.md) | 11e rendue effective (réglages lus au chargement, tests, captures, docs) | #86 fusionnée |
| 8 | [11f1.md](11f1.md) | thème des menus : chargement et données, aucun rendu changé | 11e2 fusionnée |
| 9 | [11f2.md](11f2.md) | thème des menus appliqué aux fenêtres et composants | 11f1 fusionnée |
| 10 | [11f3.md](11f3.md) | fermeture des GUI sans `delete this` | 11f2 fusionnée |
| 11 | [11f4.md](11f4.md) | icônes d'entrée et en-têtes de section | 11f3 fusionnée |
| 12 | [11f5.md](11f5.md) | clavier virtuel thémé | 11f4 fusionnée |
| 13 | [11g.md](11g.md) | carrousel système | 11f5 fusionnée |
| 14 | [11h.md](11h.md) | vues restantes | 11g fusionnée |
| 15 | [11i.md](11i.md) | thème par défaut, clôture | 11h fusionnée |

11c2 a été ajoutée après le contrôle de 11a–11c du 01/11/2026 (écarts : plan, phase 11,
« Écarts constatés »). Règles ajoutées à cette occasion : R14 à R17 (`PROMPT.md` 14.2).

11e2 et le découpage de 11f en 11f1 à 11f5 ont été ajoutés après le contrôle du 01/11/2026 :
#86 (11e) a été fusionnée sans tests ni captures et ses réglages n'ont aucun effet ; #87 (11f) n'était
qu'un squelette `MenuTheme` aux valeurs fausses. [11f.md](11f.md) est remplacé. Décision du
propriétaire du même jour : les icônes de menu, les sections et le clavier virtuel de Recalbox
sont créés (11f4, 11f5). Depuis 11e2, `phase11-check.sh` exige que la PR ajoute des `TEST_CASE`
étiquetés à son nom.

## Pour le propriétaire : contrôle d'une PR de la phase 11

Avant de fusionner, vérifier sur GitHub (10 minutes). **Un seul point manquant : ne pas
fusionner**, demander la correction. Ne jamais fusionner avec un job rouge, même pour « voir »
(#75 l'a montré : la PR suivante hérite du problème).

1. Les **4 jobs** de CI sont verts sur le **dernier** commit, et la PR ne porte **pas**
   l'étiquette `rendu-modifié` (R5, R9). Ouvrir l'artefact des captures si le job a été
   relancé.
2. La description commence par l'**inventaire** (R2) et suit les sections *Contexte*,
   *Changements*, *Réécriture sans perte*, *Vérifications effectuées*, *Critères
   d'acceptation*, *Risques / non vérifié*, *Suite prévue*.
3. *Vérifications effectuées* contient la sortie **complète** de
   `tests/smoke/phase11-check.sh <PR>` terminée par `PASS` (R17), et chaque critère coché a sa
   sortie de commande (R14) : pas de « OK » seul.
4. *Réécriture sans perte* (R16) : si un fichier existant a été largement remplacé (diff
   majoritairement rouge), le tableau liste ses anciens comportements. Le vérifier contre
   l'ancien fichier (bouton *View file* sur la base).
5. Dans l'onglet *Files changed* :
   - aucune valeur attendue modifiée dans `tests/unit/test_ThemeData.cpp` ni dans les cas
     existants des autres `test_Theme*.cpp` (R7) : seules des lignes ajoutées et des `[.]`
     retirés ;
   - aucun fichier d'un thème Recalbox (R3) ; aucun `TODO`, code commenté ou nouveau bouchon
     (R8) ;
   - dans les `.md` : pas de `<|`, pas de `/home/`, pas de commandes collées, pas de PR
     « ouverte » (R15) ;
   - chaque ligne ajoutée à `docs/THEMES.md` a une source `fichier:ligne` (R4) ;
   - `PROMPT.md` section 0 et le plan ne déclarent rien « terminé » avant fusion (R12).
6. Les critères du prompt de la PR sont tous cochés, chacun avec sa preuve. Une case non
   cochée, « tests à ajouter », « à implémenter » ou « non vérifié localement, mais CI verte »
   dans la description : **ne pas fusionner** (c'est ce qui s'est passé avec #86 et #87).
7. Le diff ne touche que les « Fichiers autorisés » du prompt, ou l'inventaire dit pourquoi.

## Pour l'agent : capture locale impossible ou écran noir

Commandes exactes : [`docs/CAPTURES.md`](../CAPTURES.md). Lire aussi `PROMPT.md` 0.3
(« Captures en local sur un bureau Wayland », « `ThemeExtras` et
`addChild` ») et 14.7. Les scripts forcent X11 ; un écran noir avec des images chargées se
diagnostique d'abord avec `draw-calls.txt` (nombre de dessins par écran, comparé à la base) et
les avertissements de `es_log.txt`. Ne jamais pousser pour « laisser la CI vérifier ».
