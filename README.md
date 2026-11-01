# Aether — synthétiseur hybride en Faust

Un instrument polyphonique ambitieux, entièrement écrit en Faust : synthèse
soustractive, FM à quatre opérateurs, synthèse additive, modulation en matrice,
séquenceur 16 pas et bus d'effets stéréo. Aucun échantillon ni plugin externe.
Le projet fournit les sources, six presets, un rendu de démonstration et des
tests hors ligne reproductibles avec **faustprobe**.

## Écouter et tester

Depuis ce dossier :

```sh
make demo                      # renders/aether-demo*.wav, 20 s, preset nebula
make demo PRESET=crystal_sequence SECONDS=24
make test                      # test-results/summary.json et résultats détaillés
make params                    # contrôles de chaque voix
faustprobe --double -I . --list-params effects.dsp # contrôles du bus partagé
```

Prérequis : `faust`, ses bibliothèques standard, `faustprobe` et Python 3.
Les scripts Python utilisent uniquement la bibliothèque standard. Le rendu brut est
un WAV float64 ; une copie `*-preview.wav`, en PCM16 normalisée à −1 dBFS,
est également produite pour une écoute facile. Cette normalisation intervient
après les mesures de faustprobe et ne fait pas partie du DSP.

```sh
make test FAUST=/usr/local/bin/faust FAUSTPROBE=/usr/local/bin/faustprobe
# Autre moteur disponible sur cette machine :
make test FAUST=/Users/letz/faust/build/bin/faust \
  FAUSTPROBE=/Users/letz/faust/tools/faustprobe/faustprobe
```

Le test d'identité demande un compilateur dont `--version` expose
`Source commit:`. Le rapport archive aussi le SHA-256 des exécutables, car
faustprobe ne fournit pas lui-même de ligne `Source commit:`.

## Jouer en temps réel

L'instrument principal expose les contrôles Faust conventionnels `freq`,
`gain` et `gate`. Un hôte polyphonique alloue les voix et additionne leurs deux
sorties. La définition `effect` s'exécute ensuite **une seule fois** sur ce mix.
Les métadonnées demandent 12 voix ; un hôte peut choisir une autre polyphonie.

Pour une application CoreAudio/Qt/MIDI sur macOS, si ses dépendances sont installées :

```sh
faust2caqt -midi -nvoices 12 -effect auto aether.dsp
```

Pour une audition autonome sans clavier, `audition.dsp` réunit une voix
régulièrement déclenchée et les effets. Elle démarre immédiatement.
Activer `sequencer` pour faire varier la hauteur. Pour un rendu hors ligne :

```sh
faustprobe --double -I . --in zero --set sequencer=1 \
  -n 480000 --sr 48000 --out renders/sequence.wav audition.dsp
```

Exemple d'accord polyphonique (notes MIDI, positions en échantillons) :

```sh
faustprobe --double -I . --in zero --nvoices 12 \
  --chord '45,52,57,60,64:100@0..96000' \
  --sr 48000 -n 384000 --fail-above 0.921 \
  --out renders/chord.wav aether.dsp
```

`aether.dsp` exécuté directement en mode monophonique produit la voix sèche.
L'hôte doit extraire `effect`, ou utiliser `audition.dsp`, pour entendre le bus.
`effects.dsp` expose ce même bus en insert autonome à deux entrées/deux sorties.

## Architecture sonore

```text
MIDI freq / gain / gate → transposition du séquenceur + matrice
    ├─ 7 voix d'unisson, chacune saw ↔ pulse/PWM ↔ triangle
    ├─ FM/PM 4 opérateurs, interpolation chaîne série / paires parallèles
    ├─ 12 partiels additifs, pente spectrale + étirement inharmonique
    └─ sinus sous-octave + bruit rose
  → wavefolder + saturation + modulation en anneau
  → morphing ladder Moog / passe-bande / passe-haut résonants
  → ADSR d'amplitude + vélocité + panoramique → sorties stéréo de la voix

Somme des voix → chorus 3 taps/canal → phaser 4 cellules
  → délai ping-pong filtré et saturé → réverbération Zita
  → largeur mid/side → compression stéréo liée
  → EQ grave/aigu + rejet DC + volume + soft clip borné à ±0,92
```

Le chorus lit des retards fractionnaires modulés et déphasés. Le ping-pong
croise ses deux retours avec amortissement et saturation dans la boucle.
Sa durée suit le tempo, jusqu'à deux temps. Zita emploie des durées de
réverbération différentes dans le grave et le médium.

Les fonctions des bibliothèques standard sont utilisées pour les oscillateurs
à alias atténué, enveloppes, filtres, phaser, compresseur et réverbération.
La FM forte, le wavefolding et la saturation peuvent produire de l'aliasing :
le projet n'implémente pas de suréchantillonnage. Les bornes de fréquence et les
fondus de partiels limitent les fréquences élevées sans prétendre l'éliminer.

## Matrice de modulation

Huit routes : `Voice/Modulation/Route_0` à `Route_7`. Chaque route possède
`source`, `destination` et `depth` (−1 à +1). Les contributions s'additionnent.
Les deux premières routes sont actives par défaut : LFO1 vers le filtre,
LFO2 vers une légère modulation de hauteur.

| Source | Signal |
| --- | --- |
| 0 | Désactivée |
| 1 | LFO1 sinus, 0,01–25 Hz |
| 2 | LFO2 triangle, 0,01–30 Hz |
| 3 | ADSR du filtre |
| 4 | Enveloppe de modulation percussive |
| 5 | Vélocité transformée en −1…+1 |
| 6 | Transposition du séquenceur divisée par 72 |
| 7 / 8 | Macro1 / Macro2 |

| Destination | Échelle pour une contribution de +1 |
| --- | --- |
| 0 | Désactivée |
| 1 | Hauteur : +12 demi-tons |
| 2 | Filtre : +4 octaves |
| 3 | Indice FM : +8 |
| 4 | Morphing oscillateur : +1 |
| 5 | Panoramique : déplacement vers la droite |
| 6 | Amplitude : multiplicateur 1 + modulation, borné à 0…2 |

Par exemple, utiliser une macro pour animer la FM :

```sh
faustprobe --double -I . --nvoices 8 --note '60:100@0..88200' \
  --set Route_2/source=7 --set Route_2/destination=3 --set Route_2/depth=0.8 \
  --at 22050 macro1=1 --at 66150 macro1=0 \
  -n 176400 --quiet aether.dsp
```

Le séquenceur avance à quatre pas par temps. Ses 16 transpositions sont
éditables (`note_0`…`note_15`) ; `sequence_octaves` multiplie ces intervalles
par 0, 1, 2 ou 3. `step_gate` règle la fraction de chaque pas tenue.
Il module la porte d'une note MIDI maintenue ; après un note-off, seule la
release subsiste. Les pas et LFO sont libres, sans remise à zéro sur note-on.

## Presets

Les valeurs sont dans `presets/patches.json`. Un hôte doit les appliquer aux
contrôles ; ce fichier n'est pas un menu de presets embarqué dans le DSP.

| Preset | Caractère |
| --- | --- |
| `nebula` | Nappe d'unisson, attaque lente, chorus et espace long |
| `glass` | Cloche FM/additive inharmonique et réverbération |
| `acid` | Séquence résonante, distorsion et delay |
| `dark_brass` | Cuivres synthétiques avec montée du filtre |
| `orbital` | Texture métallique, ring modulation, phaser et échos |
| `crystal_sequence` | Arpège hybride FM/additif, délai à croche pointée |

## Validation et limites

`tests/test_aether.py` vérifie que les patches produisent un signal stéréo
fini et audible, que le niveau reste borné, que l'inactivité et la release
reviennent au silence, et que les effets continuent après l'arrêt des voix.
Il teste 12 voix, le vol de voix, les filtres, les sources/destinations de la
matrice, des notes MIDI extrêmes, cinq fréquences d'échantillonnage, simple et
double précision. Sur les voix/effets testés séparément, il vérifie les blocs
1/7/512, le reset et le déterminisme. Le comparateur est d'abord vérifié par
une altération volontaire d'un échantillon de référence, qui doit échouer.

Pour comparer directement les échantillons des deux moteurs installés :

```sh
python3 tests/compare_engines.py /usr/local/bin/faustprobe \
  /Users/letz/faust/tools/faustprobe/faustprobe
```

Les commandes exactes et les mesures figurent dans `TEST_REPORT.md` et dans
les sorties ignorées `test-results/`. Ce sont des tests de l'instrument,
pas les gates de régression du compilateur Faust : aucun fichier de ce dernier
n'est modifié. Les vérifications ne constituent pas une preuve pour toutes
les combinaisons possibles des contrôles.

L'instrument est gourmand : les trois moteurs et les effets restent calculés
même avec leur volume/mix à zéro. Le délai utilise deux buffers de taille fixe
(1 048 576 échantillons chacun) pour couvrir 192 kHz et un tempo lent. La
polyphonie s'ajuste dans l'hôte. Le temps réel n'est pas garanti par les tests
hors ligne ; mesurer la charge dans l'hôte cible.

## Exporter vers Rust ou C++

```sh
make rust       # generated/voice.rs et generated/effects.rs
make cpp        # generated/voice.cpp et generated/effects.cpp
```

Les exports sont des classes DSP Faust : leur hôte doit gérer l'audio, le MIDI,
les voix et le bus partagé. Ce projet n'embarque pas un hôte audio Rust complet.
Les fichiers générés, rendus et résultats sont ignorés par Git.

## Version web

```sh
make web                      # voix + bus stéréo compilés en WebAssembly
make web-check                # WASM contre faustprobe + 6 presets polyphoniques
make web-serve                 # http://127.0.0.1:8088/web/
# Autre port : make web-serve PORT=8090
```

`faust-rs` est utilisé pour produire les modules WASM, comme dans
`/Users/letz/Developpements/RUST/faust-coopiano`. Un Node.js récent est requis
pour `web-check`. L'interface n'a aucune dépendance npm et ne charge aucune
ressource externe. Elle se sert en HTTP local : `file://` ne permet pas de
charger les modules et l'AudioWorklet de façon fiable.

Cliquer **Activer l’audio**, puis **Jouer un accord**, ou utiliser le clavier
virtuel. Six cartes donnent accès aux presets et rétablissent également les
valeurs par défaut des paramètres non spécifiés. Le badge « modifié » signale
les changements ; ↺ rappelle le preset. Son choix est mémorisé localement.
Les onglets donnent accès aux moteurs, ADSR/filtres, aux huit routes de
modulation, aux effets, aux seize pas et à tous les autres réglages.

Le clavier est jouable à la souris, au tactile et par positions physiques des
touches `A W S E D F T G Y H U J K` (la disposition imprimée dépend du clavier).
L'octave est réglable, la polyphonie sélectionnable entre 4, 8 et 12 voix.
La version web démarre à quatre voix pour limiter la charge de ce DSP complexe.
**Tout arrêter** réinitialise les voix et les effets ; Échap fait de même.
Les notes volées sont atténuées pendant un bloc avant réaffectation.

Le bouton MIDI demande l'accès aux entrées Web MIDI, sans SysEx. Sa disponibilité
dépend du navigateur ; Chrome/Edge conviennent généralement. Aucun microphone
n'est demandé. Le branchement d'un véritable clavier MIDI reste à vérifier
avec votre matériel. La courbe et les indicateurs de charge/voix/niveau
reflètent le moteur en cours, sans déclencher de note en arrière-plan.

`web/faust-wasm-dsp.js` est adapté du chargeur de `faust-coopiano`, avec son
contrat WASM et ses imports mathématiques. Le moteur de voix, le processeur,
l'interface et les presets sont spécifiques à Aether. Les mêmes sources Faust
servent aux versions native et web ; seul l'hôte polyphonique change.
