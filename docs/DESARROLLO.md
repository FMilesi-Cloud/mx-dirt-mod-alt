# Diario de desarrollo — MX Dirt Mod Alt

*[English version](DEVELOPMENT.md)*

Cómo se llegó desde el MX Dirt Mod 1.7.0 de kadenball a esta versión alternativa. Cada entrada cuenta qué se vio
(en el juego o en los logs), qué se encontró en el código y qué se hizo. Los números salen de los logs reales del mod
(`mx_dirt.jsonl`, `mx_dirt_render.log`), del registro de errores de Windows y de los tests.

---

## 1. Entender el mod (15/11/2026)

El mod son dos plugins de MX Bikes escritos en C:

- **`mx_dirt.dlo` (núcleo):** engancha la función de fuerza de rueda del juego (cada paso de física) y su
  planificador por frame. Con lo que lee de cada rueda (posición, carga, patinamiento, material) corre una
  simulación del suelo (hundimiento de Bekker, masa conservada, bermas, roost, humedad, baches) y publica las
  alturas en la grilla de deformación del propio juego, así la moto **realmente** pisa los surcos.
- **`mx_dirt_render.dlo` (render):** dibuja un pase de GLSL sobre el terreno que ya dibujó el juego: relieve,
  capa de tierra suelta, terrones.

Todo está atado a un `mxbikes.exe` exacto (hash SHA-256 y offsets fijos). Diagrama general:
[`diagrams/pipeline.html`](diagrams/pipeline.html).

## 2. Surcos más anchos en algunas pistas

**Lo que se veía:** en pistas con mallas distintas el surco salía más ancho.
**En el código:** el semiancho mínimo era `0,7 × celda`: el ancho lo decidía la malla, no la cubierta.
**Hecho:** `rut_width_cm` — ancho en metros, con un piso de 0,6 celdas (lo mínimo que la grilla muestra sin
"dientes"). Una línea de log `rut_width` por pista dice el tamaño de celda y si la malla limita.

Hallazgo de paso: la simulación es **siempre de 2048×2048**; en una pista de 4097 vértices (STF, 800 m) eso deja
celdas de 39 cm y surcos de 47 cm. Quedó documentado, no se cambió.

## 3. El render no dibujaba con Graphics Overhaul

**Log:** `slot_check ... reason=4`, `drawn=0`. Overhaul ya había reemplazado el puntero de `glDrawRangeElementsBaseVertex`
y el render se apagaba por diseño.
**Hecho:** encadenar: si el puntero es de otro plugin, el wrapper del dirt mod lo llama a él. Al salir solo se
restaura si el lugar todavía tiene el nuestro. En la carga siguiente se vuelve a enganchar.
**Resultado:** `reason=7` (encadenado), el render dibuja; ~2,2 ms de GPU a 2560×1440.

## 4. La moto "clavada" y el golpe de "tronco"

**Lo que se sentía:** venir tumbado, tocar un surco viejo y caerse.
**En el código:** las paredes compactadas sostenían `repose_firm × cohesión` ≈ **59°** con la humedad por defecto.
**Hecho:** `wall_angle_deg` (tope de 40°) y surco por defecto de 36 cm. Además se vio que el slider
"Deformation" del juego al 100 % **duplica** la profundidad del mod (referencia 25 %).

## 5. Pop-in del detalle y cambio de cámara

**En el código:** la capa blanda vivía en una ventana de 51 m centrada 10 m delante de la cámara: el detalle
aparecía de golpe a ~35 m y saltaba al cambiar de cámara.
**Hecho:** ventana de 128×128 tiles (102 m) centrada 25 m adelante; el tamaño quedó en macros compartidas
entre C y GLSL con un `_Static_assert`.

## 6. Menú dentro del juego (F8)

Con la **API de HUD oficial** de MX Bikes (`DrawInit`/`Draw`), la misma que usan los HUD: sin hooks nuevos.
Pestañas, teclado y mouse (cursor propio), confirmación doble para lo destructivo, escritura directa al `.ini`
(el mod ya recargaba en vivo).

**Bug propio:** el primer menú reservaba 17 textos y dibujaba 31; pisaba memoria del núcleo. Síntomas: la simulación
se cortaba y el juego crasheaba al salir de la pista (`mx_dirt.dlo+0x338bd`, en `mxb_transport_pause`). Se rastreó
la dirección reconstruyendo el orden de funciones del `.dlo` y comparándolo con `.pdata`. Corregido y con tope.

## 7. Reseteo, alisado y riego en vivo

**Trampa encontrada:** la simulación guarda alturas *relativas a la pista tal como se cargó*
(`objetivo = DFM inicial − (sim − ref)`). Poner la simulación en cero volvía al desgaste guardado.
**Hecho:** `mxdirt_height_field_rebase` reemplaza la referencia (`0` o `×0,5`) y la pista entera se reenvía por el
mismo camino que el pre-wear. Test: `tests/test_rebase.c`. Diagrama: [`diagrams/nuevas-funciones.html`](diagrams/nuevas-funciones.html).

## 8. Presets, perfiles por pista, humedad

- 6 presets de base, editables, y "guardar como…" escribiendo el nombre.
- Perfil por pista: se aplica antes de crear la sesión; al volver a una pista sin perfil se recupera la
  configuración general (`_global.ini`).
- Humedad: sigue la "pista mojada" del juego y, opcional, `OverhaulTrackWetness()` — una función que Overhaul
  exporta para otros plugins (se revisó su código de máquina: `float(void)`, solo lectura). El mod **nunca** depende
  de Overhaul.
- **Hipótesis descartada:** se supuso que el clima del juego estaba junto a la deformación en memoria (mismo orden que
  `profile.ini`). El log mostró `temperature` = 0 con 20 °C en el perfil: no era. Se sacó del menú.

## 9. Tirones al mandar terreno al juego

**Log de Overhaul:** `FREEZE ... at mx_dirt.dlo+0x316d3` (2,5 s). Rastreado a `private_range_exact`: una verificación de
seguridad que recorre la grilla entera (16 MB) **unas 5 veces por envío**. Tras un reseteo se reenvían ~1000 tiles →
segundos de congelamiento.
**Primer arreglo (este fork):** verificación completa como máximo cada 250 ms.
**Arreglo definitivo (kadenball, 1.7.1):** verificar 1/64 de la grilla por envío, rotando — mejor, porque reparte el
costo y no deja un pico periódico. Esta versión usa el suyo.

## 10. Erosión por inclinación

La inclinación sale del eje de la rueda y la normal del suelo (`lean = −eje·normal`), sin depender de hacia dónde
apunte el eje. El fondo del surco se inclina hacia adentro de la curva y la tierra cortada se empuja a la berma de
afuera. Test `tests/test_lean.c`: con 0,6 de inclinación, fondo interior −6,1 cm vs exterior −4,4 cm; berma exterior
9,5 mm vs 5,9 mm.

## 11. Cubiertas reales y telemetría

- **Motos descomprimidas:** `.cfg` → juego de cubiertas → `.tyre`: ancho (`2×TorusRadius`), `KnobsFactor` (roost) y
  `SlipSinkage`. Test con archivos reales: 450XF = 11/15 cm.
- **Motos OEM (cifradas):** el ancho se lee de la definición de la rueda en memoria, justo después del radio que el mod
  ya leía. Confirmado en el juego (Honda CRF250R OEM): `0,3415 · 0,0481 · 70,8 · 0,0001` — radio, toro, ancho y
  `radiusRPS`, el mismo orden que un `.tyre` → 9,6/14,5 cm.
- **Telemetría oficial:** frenos y caída. Se usa solo si el tamaño y los materiales de rueda coinciden con lo que el mod
  lee. Pendiente de verificar en el juego.
- Se revisó que el mod **ya** medía carga real, patinamiento e inclinación desde la memoria, así que la telemetría solo
  aporta lo que no estaba (intención de freno, caída).

## 12. El crash en código del juego

`mxbikes.exe+0x2469bb`, hilo de render, con el render del mod apagado y Overhaul activo. El ejecutable está cifrado en
disco, así que no se puede ver qué hace esa función. Queda sin atribuir; los logs ahora se guardan cada 10 s para
que un próximo caso deje rastro.

## 13. Integración de la 1.7.1 de kadenball

Fusión de tres vías (base 1.7.0, este fork, 1.7.1) sin conflictos, tras normalizar finales de línea. Entraron: las
verificaciones por porciones, el arreglo del **recuadro de suelo distinto que seguía a la moto**, los bordes con
desvanecido, `soft_lift_percent`, terrones por opacidad, `terrain_timing` y el pacing de envíos. Se revisó que su
shader no reintrodujera el tamaño de ventana 64.

## 14. Publicación

Repo público con la historia completa del original más los cambios de este fork, GPL-3.0, `NOTICE.md` con el aviso
de modificación, documentación en español e inglés y menú bilingüe. Contacto para colaborar: Discord **`fran8035`**.
