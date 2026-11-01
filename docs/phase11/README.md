# Phase 11 — prompts par PR

Prompts à donner **un par un** à l'agent de développement, dans l'ordre. Chacun se suffit à
lui-même mais renvoie au cahier des charges, [`PROMPT.md` section 14](../../PROMPT.md), qui
prime en cas d'écart.

| Ordre | Prompt | PR attendue | Prérequis |
|------:|--------|-------------|-----------|
| 1 | [11a.md](11a.md) | `docs/THEMES.md`, spécification sourcée | squelette fusionné |
| 2 | [11b.md](11b.md) | couche 1 (chargeur) | 11a fusionnée |
| 3 | [11c.md](11c.md) | couche 2 + `ThemeData` + `--check-theme` | 11b fusionnée |
| 4 | [11d.md](11d.md) | propriétés communes, captures à sondes | 11c fusionnée |
| 5 | [11e.md](11e.md) | menu « Options du thème », région, rechargement | 11d fusionnée |
| 6 | [11f.md](11f.md) | menus thémés | 11e fusionnée |
| 7 | [11g.md](11g.md) | carrousel système | 11f fusionnée |
| 8 | [11h.md](11h.md) | vues restantes | 11g fusionnée |
| 9 | [11i.md](11i.md) | thème par défaut, clôture | 11h fusionnée |

## Pour le propriétaire : contrôle d'une PR de la phase 11

Avant de fusionner, vérifier sur GitHub (5 minutes) :

1. Les **4 jobs** de CI sont verts sur le **dernier** commit, et la PR ne porte **pas**
   l'étiquette `rendu-modifié` (règle R5/R9 : aucune en phase 11).
2. La description commence par l'**inventaire** (R2) et suit les sections *Contexte*,
   *Changements*, *Vérifications effectuées*, *Risques / non vérifié*, *Suite prévue*, avec les
   sorties réelles des commandes (pas « OK » seul).
3. Dans l'onglet *Files changed* :
   - aucune valeur attendue modifiée dans `tests/unit/test_ThemeData.cpp` ni dans les cas
     existants des autres `test_Theme*.cpp` (R7) : seules des lignes ajoutées et des `[.]`
     retirés ;
   - aucun fichier d'un thème Recalbox (R3) ; aucun `TODO`, code commenté ou nouveau bouchon (R8) ;
   - `PROMPT.md` section 0 et le plan ne déclarent rien « terminé » avant fusion (R12).
4. Les critères du prompt de la PR (liste à cocher en fin de fichier) sont tous cochés dans la
   description, chacun avec sa preuve.

Si un point manque : demander la correction, ne pas fusionner « en l'état ».
