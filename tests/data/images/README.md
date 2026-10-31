# Fixtures de `test_ImageIO` et `test_TextureLoader`

Générées avec Pillow (`python3 -m pip install pillow`) :

| Fichier | Contenu |
|---------|---------|
| `still.png`, `still.webp` | 4×2, moitié haute rouge, moitié basse bleue (sans perte) |
| `still.jpg` | 16×16, même motif (JPEG qualité 95) |
| `animated.gif`, `animated.webp` | 4×4, trois images rouge, verte, bleue de 50, 100 et 200 ms |
| `logo.svg` | SVG 4×4 (rasterisé par `SVGResource`, refusé par `ImageIO::load`) |

```python
from PIL import Image
R, G, B = (255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255)
def twotone(w, h):
    im = Image.new("RGBA", (w, h))
    for y in range(h):
        for x in range(w):
            im.putpixel((x, y), R if y < h // 2 else B)
    return im
twotone(4, 2).save("still.png")
twotone(16, 16).convert("RGB").save("still.jpg", quality=95)
twotone(4, 2).save("still.webp", lossless=True)
frames = [Image.new("RGBA", (4, 4), c) for c in (R, G, B)]
frames[0].save("animated.webp", save_all=True, append_images=frames[1:], duration=[50, 100, 200], loop=0, lossless=True)
rgb = [f.convert("RGB") for f in frames]
rgb[0].save("animated.gif", save_all=True, append_images=rgb[1:], duration=[50, 100, 200], loop=0, disposal=1)
```
