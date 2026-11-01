# Captures d'écran en local

Procédure pour reproduire sur sa machine le job CI « Screenshots vs base branch » : 9 écrans
pris sous Xvfb par `tests/smoke/capture.sh`, comparés pixel par pixel avec ceux d'un autre
commit. Vérifiée chez le propriétaire le 01/11/2026 sur Arch (bureau Wayland) : 9 × `OK 0
pixels`.

## Prérequis (Arch)

```bash
sudo pacman -S --needed xorg-server-xvfb xdotool imagemagick
```

## Commandes

```bash
# 1. Binaire à tester (la branche courante)
cmake --preset relwithdebinfo && cmake --build --preset relwithdebinfo
tests/smoke/capture.sh build/relwithdebinfo/emulationstation /tmp/apres

# 2. Binaire de référence (ici main ; ou n'importe quel commit), dans un worktree à part
git fetch origin main
git worktree add /tmp/base origin/main
cmake -S /tmp/base -B /tmp/build-base -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/build-base --target emulationstation
tests/smoke/capture.sh /tmp/build-base/emulationstation /tmp/avant   # script de la branche courante, comme la CI

# 3. Comparaison : attendu 9 × "OK ... 0 pixels" puis PASS
tests/smoke/compare.sh /tmp/avant /tmp/apres

# Nettoyage
git worktree remove /tmp/base
```

Chaque `capture.sh` finit par `PASS` et affiche le nombre de dessins par écran
(`draw-calls.txt` dans le dossier de sortie). En cas d'écart, `compare.sh` écrit
`/tmp/apres/diff-<écran>.png`, qui montre les pixels différents.

## Ce qu'il ne faut pas faire

- **Ne pas piloter ES à la main avec `xvfb-run … &` puis `xdotool`.** `xdotool` lancé hors
  de `xvfb-run` parle à l'écran réel et non au serveur Xvfb. De plus, le `$!` est celui de
  `xvfb-run` et pas celui d'ES : `xdotool search --pid` ne trouve aucune fenêtre.
  `capture.sh` lance ES **et** `xdotool` dans le même `xvfb-run`.
- **Ne pas laisser SDL3 choisir Wayland.** Sur un bureau Wayland, la fenêtre s'ouvrirait sur
  le vrai bureau et la capture de Xvfb serait noire. `capture.sh` et `run.sh` font
  `unset WAYLAND_DISPLAY` et `SDL_VIDEO_DRIVER=x11` ; tout autre script qui lance ES sous
  Xvfb doit faire pareil.
- Ne jamais pousser un changement de rendu « pour que la CI vérifie » sans l'avoir capturé.

## Capture noire ou partielle

1. Comparer `draw-calls.txt` des deux dossiers : beaucoup moins de dessins côté tête = un
   composant n'est plus dessiné (exemple de #75 : extras non rattachés par `addChild`).
2. Lire les avertissements de thème `fichier:ligne` dans le journal (affiché par `capture.sh`
   en cas d'échec).
3. Vérifier que la fenêtre est bien dans Xvfb (voir plus haut).
