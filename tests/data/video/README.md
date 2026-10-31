# Fixtures de `test_VideoPlayer`

Générées avec le `ffmpeg` d'Arch (paquet `ffmpeg`) :

```sh
ffmpeg -f lavfi -i "testsrc=size=64x48:rate=10:duration=2" \
       -f lavfi -i "sine=frequency=440:duration=2:sample_rate=44100" \
       -c:v libx264 -profile:v baseline -pix_fmt yuv420p -g 10 -c:a aac -b:a 32k -shortest clip.mp4
ffmpeg -f lavfi -i "testsrc=size=64x48:rate=10:duration=1" -c:v libx264 -pix_fmt yuv420p -an silent.mp4
```

| Fichier | Contenu |
|---------|---------|
| `clip.mp4` | 64×48, 10 i/s, 2 s, H.264 + AAC (sinus 440 Hz, 44,1 kHz : rééchantillonné à 48 kHz) |
| `silent.mp4` | 64×48, 10 i/s, 1 s, H.264 sans son |
